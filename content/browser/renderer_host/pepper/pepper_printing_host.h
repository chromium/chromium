// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINTING_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINTING_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"
#include "content/common/content_export.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class CONTENT_EXPORT PepperPrintingHost : public ppapi::host::ResourceHost {
 public:
  PepperPrintingHost(
      ppapi::host::PpapiHost* host,
      PP_Instance instance,
      PP_Resource resource,
      std::unique_ptr<PepperPrintSettingsManager> print_settings_manager);
  ~PepperPrintingHost() override;

  // ppapi::host::ResourceHost implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  int32_t OnGetDefaultPrintSettings(ppapi::host::HostMessageContext* context);

  void PrintSettingsCallback(ppapi::host::ReplyMessageContext reply_context,
                             PepperPrintSettingsManager::Result result);

  std::unique_ptr<PepperPrintSettingsManager> print_settings_manager_;

  base::WeakPtrFactory<PepperPrintingHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperPrintingHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINTING_HOST_H_
