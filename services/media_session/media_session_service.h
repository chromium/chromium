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
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace media_session {

class AudioFocusManager;

class MediaSessionService : public service_manager::Service {
 public:
  MediaSessionService();
  ~MediaSessionService() override;

  // service_manager::Service:
  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceContextRefFactory* ref_factory() {
    return ref_factory_.get();
  }

 private:
  std::unique_ptr<AudioFocusManager> audio_focus_manager_;

  service_manager::BinderRegistry registry_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;
  base::WeakPtrFactory<MediaSessionService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaSessionService);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_SESSION_SERVICE_H_
