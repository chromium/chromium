// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"

namespace blink {

namespace {

const unsigned CanvasFontCacheMaxFonts = 50;
const unsigned CanvasFontCacheMaxFontsLowEnd = 5;
const unsigned CanvasFontCacheHardMaxFonts = 250;
const unsigned CanvasFontCacheHardMaxFontsLowEnd = 20;
const unsigned CanvasFontCacheHiddenMaxFonts = 1;
const int defaultFontSize = 10;

const ComputedStyle* CreateDefaultFontStyle(const Document& document) {
  const AtomicString& default_font_family = font_family_names::kSansSerif;
  FontDescription default_font_description;
  default_font_description.SetFamily(FontFamily(
      default_font_family, FontFamily::InferredTypeFor(default_font_family)));
  default_font_description.SetSpecifiedSize(defaultFontSize);
  default_font_description.SetComputedSize(defaultFontSize);
  ComputedStyleBuilder builder =
      document.IsActive()
          ? document.GetStyleResolver().CreateComputedStyleBuilder()
          : ComputedStyleBuilder(*ComputedStyle::GetInitialStyleSingleton());
  builder.SetFontDescription(default_font_description);
  return builder.TakeStyle();
}

}  // namespace

CanvasFontCache::CanvasFontCache(Document& document)
    : document_(&document),
      default_font_style_(CreateDefaultFontStyle(document)),
      pruning_scheduled_(false) {}

CanvasFontCache::~CanvasFontCache() {
}

unsigned CanvasFontCache::MaxFonts() {
  return MemoryPressureListenerRegistry::
                 IsLowEndDeviceOrPartialLowEndModeEnabledIncludingCanvasFontCache()
             ? CanvasFontCacheMaxFontsLowEnd
             : CanvasFontCacheMaxFonts;
}

unsigned CanvasFontCache::HardMaxFonts() {
  return document_->hidden()
             ? CanvasFontCacheHiddenMaxFonts
             : (MemoryPressureListenerRegistry::
                        IsLowEndDeviceOrPartialLowEndModeEnabledIncludingCanvasFontCache()
                    ? CanvasFontCacheHardMaxFontsLowEnd
                    : CanvasFontCacheHardMaxFonts);
}

bool CanvasFontCache::GetFontUsingDefaultStyle(HTMLCanvasElement& element,
                                               const String& font_string,
                                               Font& resolved_font) {
  auto it = fonts_resolved_using_default_style_.find(font_string);
  if (it != fonts_resolved_using_default_style_.end()) {
    auto list_add_result = font_lru_list_.PrependOrMoveToFirst(font_string);
    DCHECK(!list_add_result.is_new_entry);
    resolved_font = it->value->font;
    return true;
  }

  // Addition to LRU list taken care of inside ParseFont.
  MutableCSSPropertyValueSet* parsed_style = ParseFont(font_string);
  if (!parsed_style)
    return false;

  auto add_result = fonts_resolved_using_default_style_.insert(
      font_string,
      MakeGarbageCollected<FontWrapper>(document_->GetStyleEngine().ComputeFont(
          element, *default_font_style_, *parsed_style)));
  resolved_font = add_result.stored_value->value->font;
  return true;
}

MutableCSSPropertyValueSet* CanvasFontCache::ParseFont(
    const String& font_string) {
  // When the page becomes hidden it should trigger PruneAll(). In case this
  // did not happen, prune here. See crbug.com/1421699.
  if (fetched_fonts_.size() > HardMaxFonts()) {
    PruneAll();
  }

  MutableCSSPropertyValueSet* parsed_style;
  MutableStylePropertyMap::iterator i = fetched_fonts_.find(font_string);
  if (i != fetched_fonts_.end()) {
    auto add_result = font_lru_list_.PrependOrMoveToFirst(font_string);
    DCHECK(!add_result.is_new_entry);
    parsed_style = i->value;
  } else {
    parsed_style =
        CSSParser::ParseFont(font_string, document_->GetExecutionContext());
    if (!parsed_style)
      return nullptr;
    fetched_fonts_.insert(font_string, parsed_style);
    font_lru_list_.PrependOrMoveToFirst(font_string);
    // Hard limit is applied here, on the fly, while the soft limit is
    // applied at the end of the task.
    if (fetched_fonts_.size() > HardMaxFonts()) {
      DCHECK_EQ(fetched_fonts_.size(), HardMaxFonts() + 1);
      DCHECK_EQ(font_lru_list_.size(), HardMaxFonts() + 1);
      fetched_fonts_.erase(font_lru_list_.back());
      fonts_resolved_using_default_style_.erase(font_lru_list_.back());
      font_lru_list_.pop_back();
    }
  }
  SchedulePruningIfNeeded();

  return parsed_style;
}

void CanvasFontCache::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK(pruning_scheduled_);
  DCHECK(main_cache_purge_preventer_);
  while (fetched_fonts_.size() > std::min(MaxFonts(), HardMaxFonts())) {
    fetched_fonts_.erase(font_lru_list_.back());
    fonts_resolved_using_default_style_.erase(font_lru_list_.back());
    font_lru_list_.pop_back();
  }
  main_cache_purge_preventer_.reset();
  Thread::Current()->RemoveTaskObserver(this);
  pruning_scheduled_ = false;
}

void CanvasFontCache::SchedulePruningIfNeeded() {
  if (pruning_scheduled_)
    return;
  DCHECK(!main_cache_purge_preventer_);
  main_cache_purge_preventer_ = std::make_unique<FontCachePurgePreventer>();
  Thread::Current()->AddTaskObserver(this);
  pruning_scheduled_ = true;
}

bool CanvasFontCache::IsInCache(const String& font_string) const {
  return fetched_fonts_.find(font_string) != fetched_fonts_.end();
}

unsigned int CanvasFontCache::GetCacheSize() const {
  return fetched_fonts_.size();
}

void CanvasFontCache::PruneAll() {
  fetched_fonts_.clear();
  font_lru_list_.clear();
  fonts_resolved_using_default_style_.clear();
}

void CanvasFontCache::Trace(Visitor* visitor) const {
  visitor->Trace(fonts_resolved_using_default_style_);
  visitor->Trace(fetched_fonts_);
  visitor->Trace(document_);
  visitor->Trace(default_font_style_);
}

void CanvasFontCache::Dispose() {
  main_cache_purge_preventer_.reset();
  if (pruning_scheduled_) {
    Thread::Current()->RemoveTaskObserver(this);
  }
}

}  // namespace blink
