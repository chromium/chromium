// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_util.h"

#import <algorithm>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/ui/autofill_ai/entity_source_attribution_utils.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

using Source =
    autofill::EntityInstance::PersonalContextRecordTypePayload::Source;

// Size of the symbol icon displayed next to each source item.
constexpr CGFloat kSourceIconPointSize = 20.0;

// Returns the icon for the given source `type`.
UIImage* GetSourceIcon(Source::Type type) {
  switch (type) {
    case Source::Type::kGmail:
      return SymbolWithPointSize(SymbolMailFill, kSourceIconPointSize);
    case Source::Type::kPhotos:
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      return SymbolWithPointSize(SymbolGooglePhotos, kSourceIconPointSize);
#else
      return SymbolWithPointSize(SymbolPhoto, kSourceIconPointSize);
#endif
  }
  NOTREACHED();
}

}  // namespace

BOOL EntityHasValidSources(const autofill::EntityInstance& entity) {
  const auto* payload =
      std::get_if<autofill::EntityInstance::PersonalContextRecordTypePayload>(
          &entity.record_type_data());
  if (!payload) {
    return NO;
  }
  return std::ranges::any_of(payload->sources, [](const Source& source) {
    return source.url.is_valid();
  });
}

NSArray<AutofillAiSourceGroup*>* ExtractSourcesFromEntity(
    const autofill::EntityInstance& entity) {
  const auto* payload =
      std::get_if<autofill::EntityInstance::PersonalContextRecordTypePayload>(
          &entity.record_type_data());
  if (!payload) {
    return @[];
  }

  NSMutableArray<AutofillAiSourceItem*>* gmail_items = [NSMutableArray array];
  NSMutableArray<AutofillAiSourceItem*>* photos_items = [NSMutableArray array];

  for (const Source& source : payload->sources) {
    if (!source.url.is_valid()) {
      continue;
    }
    AutofillAiSourceItem* item = [[AutofillAiSourceItem alloc]
        initWithTitle:base::SysUTF16ToNSString(
                          autofill::GetSourceDisplayTitle(source))
             subtitle:nil
                  URL:source.url
                 type:source.type()
                 icon:GetSourceIcon(source.type())];
    switch (source.type()) {
      case Source::Type::kGmail:
        [gmail_items addObject:item];
        break;
      case Source::Type::kPhotos:
        [photos_items addObject:item];
        break;
    }
  }

  NSMutableArray<AutofillAiSourceGroup*>* groups = [NSMutableArray array];
  if (gmail_items.count > 0) {
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_GMAIL_SECTION_TITLE);
    [groups
        addObject:[[AutofillAiSourceGroup alloc] initWithTitle:title
                                                         items:gmail_items]];
  }
  if (photos_items.count > 0) {
    NSString* title = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE);
    [groups
        addObject:[[AutofillAiSourceGroup alloc] initWithTitle:title
                                                         items:photos_items]];
  }
  return groups;
}

NSString* SourcesHeaderSubtitle(const autofill::EntityInstance& entity,
                                NSString* suggestion_value) {
  NSString* entity_type_name =
      base::SysUTF16ToNSString(entity.type().GetNameForI18n());
  if (suggestion_value.length > 0 && entity_type_name.length > 0) {
    return [NSString
        stringWithFormat:@"%@ · %@", entity_type_name, suggestion_value];
  }
  if (entity_type_name.length > 0) {
    return entity_type_name;
  }
  return suggestion_value ?: @"";
}
