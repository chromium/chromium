# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Information about why a test failed.
# Modelled after https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/proto/v1/failure_reason.proto
class FailureReason(object):
    def __init__(self, primary_error_message):
        """Initialises a new failure reason.

        Args:
            primary_error_message: The error message that ultimately caused
                    the test to fail. This should/ only be the error message
                    and should not include any stack traces. In the case that
                    a test failed due to multiple expectation failures, any
                    immediately fatal failure should be chosen, or otherwise
                    the first expectation failure.
        """
        self.primary_error_message = primary_error_message
