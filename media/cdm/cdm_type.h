// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_TYPE_H_
#define MEDIA_CDM_CDM_TYPE_H_

#include "base/token.h"

namespace media {

// A token to uniquely identify the type of the CDM. Used for per-CDM-type
// isolation, e.g. for running different CDMs in different child processes,
// and per-CDM-type storage. A zero token indicates that this CdmType should
// not have a corresponding CdmStorage. Note that the 'CdmType' has no external
// dependencies (e.g specs), and are chosen to be unique for the reasons stated
// above.
using CdmType = base::Token;

}  // namespace media

#endif  // MEDIA_CDM_CDM_TYPE_H_
