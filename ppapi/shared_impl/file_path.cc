// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_path.h"

namespace ppapi {

PepperFilePath::PepperFilePath() : domain_(DOMAIN_INVALID), path_() {}

PepperFilePath::PepperFilePath(Domain domain, const base::FilePath& path)
    : domain_(domain), path_(path) {
  // TODO(viettrungluu): Should we DCHECK() some things here?
}

}  // namespace ppapi
