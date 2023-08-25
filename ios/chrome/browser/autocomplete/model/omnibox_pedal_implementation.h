// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_PEDAL_IMPLEMENTATION_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_PEDAL_IMPLEMENTATION_H_

#include <unordered_map>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

// Returns the full set of encapsulated OmniboxPedal implementations.
std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool testing);

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_PEDAL_IMPLEMENTATION_H_
