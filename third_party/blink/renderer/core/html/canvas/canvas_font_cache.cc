// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"

namespace {

const unsigned CanvasFontCacheMaxFonts = 50;
const unsigned CanvasFontCacheMaxFontsLowEnd = 5;
const unsigned CanvasFontCacheHardMaxFonts = 250;
const unsigned CanvasFontCacheHardMaxFontsLowEnd = 20;
const unsigned CanvasFontCacheHiddenMaxFonts = 1;
const int defaultFontSize = 10;
const char defaultFontFamily[] = "sans-serif";
}

namespace blink {

CanvasFontCache::CanvasFontCache(Document& document)
    : document_(&document), pruning_scheduled_(false) {
  FontFamily font_family;
  font_family.SetFamily(defaultFontFamily);
  FontDescription default_font_description;
  default_font_description.SetFamily(font_family);
  default_font_description.SetSpecifiedSize(defaultFontSize);
  default_font_description.SetComputedSize(defaultFontSize);
  default_font_style_ = ComputedStyle::Create();
  default_font_style_->SetFontDescription(default_font_description);
  default_font_style_->GetFont().Update(
      default_font_style_->GetFont().GetFontSelector());
}

CanvasFontCache::~CanvasFontCache() {
}

unsigned CanvasFontCache::MaxFonts() {
  return MemoryCoordinator::IsLowEndDevice() ? CanvasFontCacheMaxFontsLowEnd
                                             : CanvasFontCacheMaxFonts;
}

unsigned CanvasFontCache::HardMaxFonts() {
  return document_->hidden() ? CanvasFontCacheHiddenMaxFonts
                             : (MemoryCoordinator::IsLowEndDevice()
                                    ? CanvasFontCacheHardMaxFontsLowEnd
                                    : CanvasFontCacheHardMaxFonts);
}

bool CanvasFontCache::GetFontUsingDefaultStyle(const String& font_string,
                                               Font& resolved_font) {
  HashMap<String, Font>::iterator i =
      fonts_resolved_using_default_style_.find(font_string);
  if (i != fonts_resolved_using_default_style_.end()) {
    DCHECK(font_lru_list_.Contains(font_string));
    font_lru_list_.erase(font_string);
    font_lru_list_.insert(font_string);
    resolved_font = i->value;
    return true;
  }

  // Addition to LRU list taken care of inside parseFont
  MutableCSSPropertyValueSet* parsed_style = ParseFont(font_string);
  if (!parsed_style)
    return false;

  scoped_refptr<ComputedStyle> font_style =
      ComputedStyle::Clone(*default_font_style_.get());
  document_->EnsureStyleResolver().ComputeFont(font_style.get(), *parsed_style);
  fonts_resolved_using_default_style_.insert(font_string,
                                             font_style->GetFont());
  resolved_font = fonts_resolved_using_default_style_.find(font_string)->value;
  return true;
}

MutableCSSPropertyValueSet* CanvasFontCache::ParseFont(
    const String& font_string) {
  MutableCSSPropertyValueSet* parsed_style;
  MutableStylePropertyMap::iterator i = fetched_fonts_.find(font_string);
  if (i != fetched_fonts_.end()) {
    DCHECK(font_lru_list_.Contains(font_string));
    parsed_style = i->value;
    font_lru_list_.erase(font_string);
    font_lru_list_.insert(font_string);
  } else {
    parsed_style = MutableCSSPropertyValueSet::Create(kHTMLStandardMode);
    CSSParser::ParseValue(parsed_style, CSSPropertyFont, font_string, true,
                          document_->GetSecureContextMode());
    if (parsed_style->IsEmpty())
      return nullptr;
    // According to
    // http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit", "initial" and "unset" values must be ignored.
    const CSSValue* font_value =
        parsed_style->GetPropertyCSSValue(CSSPropertyFontSize);
    if (font_value && font_value->IsCSSWideKeyword())
      return nullptr;
    fetched_fonts_.insert(font_string, parsed_style);
    font_lru_list_.insert(font_string);
    // Hard limit is applied here, on the fly, while the soft limit is
    // applied at the end of the task.
    if (fetched_fonts_.size() > HardMaxFonts()) {
      DCHECK_EQ(fetched_fonts_.size(), HardMaxFonts() + 1);
      DCHECK_EQ(font_lru_list_.size(), HardMaxFonts() + 1);
      fetched_fonts_.erase(font_lru_list_.front());
      fonts_resolved_using_default_style_.erase(font_lru_list_.front());
      font_lru_list_.RemoveFirst();
    }
  }
  SchedulePruningIfNeeded();

  return parsed_style;
}

void CanvasFontCache::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK(pruning_scheduled_);
  DCHECK(main_cache_purge_preventer_);
  while (fetched_fonts_.size() > MaxFonts()) {
    fetched_fonts_.erase(font_lru_list_.front());
    fonts_resolved_using_default_style_.erase(font_lru_list_.front());
    font_lru_list_.RemoveFirst();
  }
  main_cache_purge_preventer_.reset();
  Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
  pruning_scheduled_ = false;
}

void CanvasFontCache::SchedulePruningIfNeeded() {
  if (pruning_scheduled_)
    return;
  DCHECK(!main_cache_purge_preventer_);
  main_cache_purge_preventer_ = std::make_unique<FontCachePurgePreventer>();
  Platform::Current()->CurrentThread()->AddTaskObserver(this);
  pruning_scheduled_ = true;
}

bool CanvasFontCache::IsInCache(const String& font_string) {
  return fetched_fonts_.find(font_string) != fetched_fonts_.end();
}

void CanvasFontCache::PruneAll() {
  fetched_fonts_.clear();
  font_lru_list_.clear();
  fonts_resolved_using_default_style_.clear();
}

void CanvasFontCache::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetched_fonts_);
  visitor->Trace(document_);
}

void CanvasFontCache::Dispose() {
  main_cache_purge_preventer_.reset();
  if (pruning_scheduled_) {
    Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
  }
}

}  // namespace blink
