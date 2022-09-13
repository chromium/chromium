// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/fuchsia_dir_scheme.h"

#include "url/url_util.h"

const char kFuchsiaDirScheme[] = "fuchsia-dir";

void RegisterFuchsiaDirScheme() {
  url::AddStandardScheme(kFuchsiaDirScheme, url::SCHEME_WITH_HOST);
  url::AddLocalScheme(kFuchsiaDirScheme);
}
