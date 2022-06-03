// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/fuchsia_dir_scheme.h"

#include "url/url_util.h"

namespace cr_fuchsia {

const char kFuchsiaDirScheme[] = "fuchsia-dir";

void RegisterFuchsiaDirScheme() {
  url::AddStandardScheme(kFuchsiaDirScheme, url::SCHEME_WITH_HOST);
  url::AddLocalScheme(kFuchsiaDirScheme);
}

}  // namespace cr_fuchsia
