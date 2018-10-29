// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_DATA_RESOURCE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_DATA_RESOURCE_HELPER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Helper functions providing access to ui::ResourceBundle in Blink.

// Returns the contents of a resource as a string specified by the
// resource name.
PLATFORM_EXPORT String GetDataResourceAsASCIIString(const char* resource);

// Returns the contents of a resource as a string specified by the
// resource id from Grit.
PLATFORM_EXPORT String GetResourceAsString(int resource_id);

// Uncompresses a gzipped resource and returns it as a string. The resource
// is specified by the resource id from Grit.
PLATFORM_EXPORT String UncompressResourceAsString(int resource_id);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_DATA_RESOURCE_HELPER_H_
