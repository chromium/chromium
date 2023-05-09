// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_session_service.h"

#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/media_session_service_impl.h"
#include "services/media_session/public/cpp/features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "services/media_session/public/cpp/media_session_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace content {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace {

class LacrosMediaSessionServiceImpl
    : public media_session::MediaSessionService {
 public:
  LacrosMediaSessionServiceImpl() = default;
  ~LacrosMediaSessionServiceImpl() override = default;
  LacrosMediaSessionServiceImpl(const LacrosMediaSessionServiceImpl&) = delete;
  LacrosMediaSessionServiceImpl& operator=(
      const LacrosMediaSessionServiceImpl&) = delete;

  void BindAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service &&
        lacros_service
            ->IsSupported<media_session::mojom::AudioFocusManager>()) {
      lacros_service->BindAudioFocusManager(std::move(receiver));
    }
  }

  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service &&
        lacros_service
            ->IsSupported<media_session::mojom::AudioFocusManagerDebug>()) {
      lacros_service->BindAudioFocusManagerDebug(std::move(receiver));
    }
  }

  void BindMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service &&
        lacros_service
            ->IsSupported<media_session::mojom::MediaControllerManager>()) {
      lacros_service->BindMediaControllerManager(std::move(receiver));
    }
  }
};

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

media_session::MediaSessionService& GetMediaSessionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service
          ->IsSupported<media_session::mojom::MediaControllerManager>() &&
      lacros_service
          ->IsSupported<media_session::mojom::AudioFocusManagerDebug>() &&
      lacros_service->IsSupported<media_session::mojom::AudioFocusManager>()) {
    static base::NoDestructor<LacrosMediaSessionServiceImpl> service;
    return *service;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  static base::NoDestructor<media_session::MediaSessionServiceImpl> service;
  return *service;
}

}  // namespace content
