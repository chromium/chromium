// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace network {

// The default Accept header value to use if none were specified.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kDefaultAcceptHeaderValue[];

// The directory name of the database in the shared dictionary directory which
// is specified by `shared_dictionary_directory` field of NetworkContextParams.
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FilePath::CharType kSharedDictionaryDbDirName[];

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
