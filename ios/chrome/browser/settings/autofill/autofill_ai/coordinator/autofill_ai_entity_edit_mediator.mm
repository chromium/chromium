// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"

@implementation AutofillAIEntityEditMediator {
  // The entity instance being viewed and edited.
  std::optional<autofill::EntityInstance> _entityInstance;

  // The entity data manager. It outlives the mediator.
  raw_ptr<autofill::EntityDataManager> _entityDataManager;
}

@synthesize consumer = _consumer;

- (instancetype)initWithEntityInstance:(autofill::EntityInstance)entityInstance
                     entityDataManager:
                         (autofill::EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    _entityInstance = std::move(entityInstance);
    _entityDataManager = entityDataManager;
  }
  return self;
}

- (void)setConsumer:(id<AutofillAIEntityEditConsumer>)consumer {
  _consumer = consumer;
  if (consumer && _entityInstance) {
    [consumer setEntityInstance:*_entityInstance];
  }
}

@end
