# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class BuganizerClientMock:
    def NewIssue(self,
                 title="none",
                 description="none",
                 project='chromium',
                 priority='P3',
                 severity='S3',
                 components=None,
                 owner=None,
                 cc=None,
                 status=None,
                 componentId=None):
        return
