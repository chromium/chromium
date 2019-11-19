// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_
#define SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace media_session {

class AudioFocusManager;

class MediaSessionService : public service_manager::Service {
 public:
  explicit MediaSessionService(service_manager::mojom::ServiceRequest request);
  ~MediaSessionService() override;

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  const AudioFocusManager& audio_focus_manager_for_testing() const {
    return *audio_focus_manager_.get();
  }

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  std::unique_ptr<AudioFocusManager> audio_focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionService);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_
