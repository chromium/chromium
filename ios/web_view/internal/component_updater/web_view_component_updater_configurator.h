// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_COMPONENT_UPDATER_WEB_VIEW_COMPONENT_UPDATER_CONFIGURATOR_H_
#define IOS_WEB_VIEW_INTERNAL_COMPONENT_UPDATER_WEB_VIEW_COMPONENT_UPDATER_CONFIGURATOR_H_

#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"

namespace base {
class CommandLine;
}

namespace ios_web_view {

// Returns a configurator for updating components.
// See similar implementation at
// //ios/chrome/browser/component_updater/model/ios_component_updater_configurator.h
scoped_refptr<update_client::Configurator> MakeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline);

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_COMPONENT_UPDATER_WEB_VIEW_COMPONENT_UPDATER_CONFIGURATOR_H_
