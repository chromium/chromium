// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LOCALIZED_STRINGS_H_
#define MEDIA_BASE_LOCALIZED_STRINGS_H_

#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

// The media layer can't access Chrome's resource bundle directly. This facility
// allows clients to provide indirect access.

// IDs that will get mapped to corresponding entries with IDS_ prefixes in
// chrome/app/generated_resources.grd.
enum MessageId {
  DEFAULT_AUDIO_DEVICE_NAME,
#if BUILDFLAG(IS_WIN)
  COMMUNICATIONS_AUDIO_DEVICE_NAME,
#endif
};

// Implementations are expected to convert MessageIds to generated_resources.grd
// IDs and extract the matching string from Chrome's resource bundle (e.g.
// through l10n_util::GetStringUTF16).
using LocalizedStringProvider = std::u16string (*)(MessageId message_id);

// Initializes the global LocalizedStringProvider function.
MEDIA_EXPORT void SetLocalizedStringProvider(LocalizedStringProvider func);

// The LocalizedStringProvider has probably not been initialized on iOS. This
// will give an early compile warning for clients attempting to use it.

// Returns a resource string corresponding to |message_id|. See l10n_util.h.
// Returns an empty string if the LocalizedStringProvider has not been
// initialized or if the ID is unrecognized.
MEDIA_EXPORT std::string GetLocalizedStringUTF8(MessageId message_id);
std::u16string GetLocalizedStringUTF16(MessageId message_id);

}  // namespace media

#endif  // MEDIA_BASE_LOCALIZED_STRINGS_H_
