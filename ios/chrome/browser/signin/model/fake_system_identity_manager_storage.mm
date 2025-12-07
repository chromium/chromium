// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_manager_storage.h"

#import <iterator>
#import <ranges>

#import "base/check.h"
#import "base/containers/span.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_details.h"

@implementation FakeSystemIdentityManagerStorage {
  // Stores the key in insertion order.
  std::vector<GaiaId> _orderedKeys;

  // Stores details about the identities, keyed by gaia id.
  base::flat_map<GaiaId, FakeSystemIdentityDetails*> _details;

  // Counter incremented when the structure is mutated. Used to detect that
  // the storage has been mutated during iteration by NSFastEnumeration.
  unsigned long _mutations;
}

- (instancetype)init {
  if ((self = [super init])) {
    _mutations = 0;
  }
  return self;
}

- (BOOL)containsIdentityWithGaiaID:(const GaiaId&)gaiaID {
  return [self detailsForGaiaID:gaiaID] != nil;
}

- (FakeSystemIdentityDetails*)detailsForGaiaID:(const GaiaId&)gaiaID {
  return _details[gaiaID];
}

- (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  GaiaId key = fakeIdentity.gaiaId;
  if (_details[key]) {
    return;
  }

  // Key is not found in _orderedKeys.
  DCHECK(std::find(_orderedKeys.begin(), _orderedKeys.end(), key) ==
         _orderedKeys.end());

  _details[key] =
      [[FakeSystemIdentityDetails alloc] initWithFakeIdentity:fakeIdentity];
  _orderedKeys.push_back(key);

  // Storage has changed, invalidate current iterations.
  ++_mutations;
}

- (void)removeIdentityWithGaiaID:(const GaiaId&)gaiaID {
  if (!_details.contains(gaiaID)) {
    return;
  }

  _details.erase(gaiaID);

  int number_of_erasure = std::erase(_orderedKeys, gaiaID);
  DCHECK(number_of_erasure);

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

  // SAFETY: -countByEnumaratingWithState:objects:count: is called by the
  // Objective-C runtime to implement fast enumeration (i.e. the "for in"
  // loop of Objective-C) and guarantee that `buffer` can store at least
  // `len` values.
  base::span<id __unsafe_unretained> output =
      UNSAFE_BUFFERS(base::span(buffer, len));

  // Iterate over as many object as possible, starting from
  // previously recorded position.
  NSUInteger count = 0;
  NSUInteger start = state->extra[0];
  if (start < _orderedKeys.size()) {
    count = std::min(len, _orderedKeys.size() - start);

    base::span<GaiaId> view = base::span(_orderedKeys).subspan(start, count);
    std::ranges::transform(view, output.begin(),
                           [&](GaiaId key) { return _details[key]; });
  }

  // Update iteration state.
  state->extra[0] = start + count;
  state->itemsPtr = buffer;

  return count;
}

@end
