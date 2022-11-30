// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RESOURCES_H_
#define REMOTING_HOST_RESOURCES_H_

#include <string>

namespace remoting {

// Loads (or reloads) Chromoting resources for the given locale. |pref_locale|
// is passed to l10n_util::GetApplicationLocale(), so the default system locale
// is used if |pref_locale| is empty. Returns |true| if the shared resource
// bundle has been initialized.
bool LoadResources(const std::string& pref_locale);

// Unloads Chromoting resources.
void UnloadResources();

}  // namespace remoting

#endif  // REMOTING_HOST_RESOURCES_H_
