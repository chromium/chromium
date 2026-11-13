// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/common/default_apps_for_testing.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace webapps {

namespace {

int g_disable_count = 0;
bool g_force_enable_for_testing = false;

void CheckCalledOnValidSequence() {
#if DCHECK_IS_ON()
  static base::NoDestructor<base::SequenceChecker> sequence_checker;
  DCHECK_CALLED_ON_VALID_SEQUENCE(*sequence_checker);
#endif
}

}  // namespace

base::ScopedClosureRunner DisableDefaultAppsForTesting() {
  CheckCalledOnValidSequence();
  ++g_disable_count;
  return base::ScopedClosureRunner(base::BindOnce([]() {
    CheckCalledOnValidSequence();
    DCHECK_GT(g_disable_count, 0);
    --g_disable_count;
  }));
}

base::AutoReset<bool> EnableDefaultAppsForTesting() {
  CheckCalledOnValidSequence();
  return base::AutoReset<bool>(&g_force_enable_for_testing, true);
}

bool AreDefaultAppsDisabledForTesting() {
  CheckCalledOnValidSequence();
  return g_disable_count > 0 && !g_force_enable_for_testing;
}

}  // namespace webapps
