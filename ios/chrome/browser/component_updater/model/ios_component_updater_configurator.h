// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPONENT_UPDATER_MODEL_IOS_COMPONENT_UPDATER_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_COMPONENT_UPDATER_MODEL_IOS_COMPONENT_UPDATER_CONFIGURATOR_H_

#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"

namespace base {
class CommandLine;
}

namespace component_updater {

scoped_refptr<update_client::Configurator> MakeIOSComponentUpdaterConfigurator(
    const base::CommandLine* cmdline);

}  // namespace component_updater

#endif  // IOS_CHROME_BROWSER_COMPONENT_UPDATER_MODEL_IOS_COMPONENT_UPDATER_CONFIGURATOR_H_
