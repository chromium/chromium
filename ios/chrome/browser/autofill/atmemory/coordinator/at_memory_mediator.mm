// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"

using autofill::AtMemoryManager;
using autofill::BrowserAutofillManager;
using autofill::FieldGlobalId;
using autofill::FormGlobalId;
using autofill::FormStructure;
using autofill::IsAsync;
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
  [self.atMemoryHandler dismissAtMemory];
}

- (void)fillWithSuggestion:(const Suggestion&)suggestion {
  if (!_atMemoryManager || !_autofillManager) {
    return;
  }

  const FormStructure* form = _autofillManager->FindCachedFormById(_fieldId);
  FormGlobalId formId = form ? form->global_id() : FormGlobalId();

  IsAsync isAsync = _atMemoryManager->FillSearchResult(
      /*bam=*/*_autofillManager,
      /*form_id=*/formId,
      /*field_id=*/_fieldId,
      /*suggestion=*/suggestion,
      /*metadata=*/{});
  if (!isAsync.value()) {
    [self.atMemoryHandler dismissAtMemory];
  }
}

@end
