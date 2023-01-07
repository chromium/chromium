// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_WIN_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_WIN_H_

#include <stdint.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"

namespace remoting {

class WtsSessionChangeObserver;

// Windows implementation of UrlForwarderConfigurator.
// Note that this class requires elevated privileges.
class UrlForwarderConfiguratorWin final : public UrlForwarderConfigurator {
 public:
  UrlForwarderConfiguratorWin();
  ~UrlForwarderConfiguratorWin() override;

  void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) override;
  void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) override;

  UrlForwarderConfiguratorWin(const UrlForwarderConfiguratorWin&) = delete;
  UrlForwarderConfiguratorWin& operator=(const UrlForwarderConfiguratorWin&) =
      delete;

 private:
  void OnSetUpResponse(bool success);
  void OnReportUserInterventionRequired();
  void OnWtsSessionChange(uint32_t event, uint32_t session_id);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // For uncurtained sessions, Windows reuses the same console session before
  // and after the user logs in. So we use these members to delay checking the
  // URL forwarder state until the user is logged in. They are valid (non-null)
  // only when the session is not logged in.
  // For curtained sessions, Windows creates a new session whenever the user
  // logs in, in which case the desktop process will be destroyed along with the
  // URL configurator and the pending requests, and a new desktop process will
  // be launched and IsUrlForwarderSetUp() will be called with a valid user
  // context.
  std::unique_ptr<WtsSessionChangeObserver> wts_session_change_observer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  IsUrlForwarderSetUpCallback is_url_forwarder_set_up_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SetUpUrlForwarderCallback set_up_url_forwarder_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer report_user_intervention_required_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UrlForwarderConfiguratorWin> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_WIN_H_
