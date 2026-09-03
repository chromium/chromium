// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import <variant>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"

using autofill::AtMemoryManager;
using autofill::BrowserAutofillManager;
using autofill::FieldGlobalId;
using autofill::FormGlobalId;
using autofill::IsSpiiMemoryDataType;
using autofill::Suggestion;

@implementation AtMemoryMediator {
  // Manager for AtMemory operations.
  raw_ptr<AtMemoryManager> _atMemoryManager;
  // Manager for Browser Autofill operations.
  raw_ptr<BrowserAutofillManager> _autofillManager;
  // Injector for manual fill data.
  __weak id<ManualFillContentInjector> _contentInjector;
  // Field ID that initiated AtMemory.
  FieldGlobalId _fieldId;
}

- (instancetype)initWithAtMemoryManager:(AtMemoryManager*)atMemoryManager
                        autofillManager:(BrowserAutofillManager*)autofillManager
                        contentInjector:
                            (id<ManualFillContentInjector>)contentInjector
                                fieldId:(FieldGlobalId)fieldId {
  self = [super init];
  if (self) {
    CHECK(atMemoryManager);
    CHECK(autofillManager);
    _atMemoryManager = atMemoryManager;
    _autofillManager = autofillManager;
    _contentInjector = contentInjector;
    _fieldId = fieldId;
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  _atMemoryManager = nullptr;
  _autofillManager = nullptr;
  _contentInjector = nil;
}

#pragma mark - AtMemoryFillCommands

- (void)fillWithContent:(NSString*)content {
  [_contentInjector userDidPickContent:content
                         passwordField:NO
                         requiresHTTPS:YES
                       jumpToNextField:NO
                            actionType:autofill::mojom::FieldActionType::
                                           kReplaceSelectionForAtMemory];
}

- (void)fillWithSuggestion:(const Suggestion&)suggestion {
  const Suggestion::AtMemoryPayload* payload =
      std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload);
  bool is_obfuscated =
      payload && IsSpiiMemoryDataType(payload->memory_data_type);

  if (is_obfuscated) {
    [self fillObfuscatedSuggestion:suggestion];
  } else {
    NSString* value = nil;
    if (payload && !payload->value.empty()) {
      value = base::SysUTF16ToNSString(payload->value);
    } else {
      value = base::SysUTF16ToNSString(suggestion.main_text.value);
    }
    [self fillWithContent:value];
  }
}

#pragma mark - Private

- (void)fillObfuscatedSuggestion:(const Suggestion&)suggestion {
  if (!_atMemoryManager || !_autofillManager) {
    return;
  }
  _atMemoryManager->FillSearchResult(
      /*bam=*/*_autofillManager,
      /*form_id=*/FormGlobalId(),
      /*field_id=*/_fieldId,
      /*suggestion=*/suggestion,
      /*metadata=*/{});
}

@end
