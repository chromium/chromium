// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_manager_storage.h"

#import "base/check.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_details.h"

@implementation FakeSystemIdentityManagerStorage {
  // Stores the key in insertion order.
  __strong NSMutableArray<NSString*>* _orderedKeys;

  // Stores details about the identities, keyed by gaia id.
  __strong NSMutableDictionary<NSString*, FakeSystemIdentityDetails*>* _details;

  // Counter incremented when the structure is mutated. Used to detect that
  // the storage has been mutated during iteration by NSFastEnumeration.
  unsigned long _mutations;
}

- (instancetype)init {
  if ((self = [super init])) {
    _orderedKeys = [[NSMutableArray alloc] init];
    _details = [[NSMutableDictionary alloc] init];
    _mutations = 0;
  }
  return self;
}

- (BOOL)containsIdentityWithGaiaID:(NSString*)gaiaID {
  return [self detailsForGaiaID:gaiaID] != nil;
}

- (FakeSystemIdentityDetails*)detailsForGaiaID:(NSString*)gaiaID {
  return [_details objectForKey:gaiaID];
}

- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  NSString* key = fakeIdentity.gaiaID;
  if ([_details objectForKey:key])
    return;

  DCHECK(![_orderedKeys containsObject:key]);
  _details[key] =
      [[FakeSystemIdentityDetails alloc] initWithFakeIdentity:fakeIdentity];
  [_orderedKeys addObject:key];

  // Storage has changed, invalidate current iterations.
  ++_mutations;
}

- (void)removeIdentityWithGaiaID:(NSString*)gaiaID {
  if (![_details objectForKey:gaiaID]) {
    return;
  }

  DCHECK([_orderedKeys containsObject:gaiaID]);
  [_details removeObjectForKey:gaiaID];
  [_orderedKeys removeObject:gaiaID];

  // Storage has changed, invalidate current iterations.
  ++_mutations;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained[])buffer
                                    count:(NSUInteger)len {
  // `state` is set to zero by the runtime before the enumeration starts.
  // Initialize the structure and mark it as such by changing the `state`
  // value.
  if (state->state == 0) {
    state->state = 1;
    state->extra[0] = 0;  // Index of iteration position in _orderedKeys.
    state->mutationsPtr = &_mutations;  // Detect mutations.
  }

  // Iterate over as many object as possible, starting from
  // previously recorded position.
  NSUInteger index;
  NSUInteger start = state->extra[0];
  NSUInteger count = _orderedKeys.count;
  for (index = 0; index < len && start + index < count; ++index) {
    NSString* key = [_orderedKeys objectAtIndex:(start + index)];
    buffer[index] = [_details objectForKey:key];
  }

  // Update iteration state.
  state->extra[0] = start + index;
  state->itemsPtr = buffer;

  return index;
}

@end
