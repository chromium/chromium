// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_
#define REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "remoting/host/url_forwarder_configurator.h"

namespace remoting {

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

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  SetUpUrlForwarderCallback set_up_url_forwarder_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer report_user_intervention_required_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UrlForwarderConfiguratorWin> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_
