// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_service.h"

namespace midi {

MidiManager* MidiManager::Create(MidiService* service) {
  // TODO(crbug.com/391914246): Implement the MidiManager when tvOS actually
  // requires the functionality.
  NOTREACHED();
}

}  // namespace midi
