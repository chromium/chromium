# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib


class MockChromiumCommit:

    def __init__(self,
                 host,
                 position='refs/heads/master@{#123}',
                 change_id='Iba5eba11',
                 author='Fake author',
                 subject='Fake subject',
                 body='Fake body',
                 patch='Fake patch contents'):
        self.host = host
        self.position = position
        self.sha = hashlib.sha1(position.encode('utf-8')).hexdigest()
        self._change_id = change_id
        self._author = author
        self._subject = subject
        self._body = body
        self._patch = patch

    def __str__(self):
        return f'{self.short_sha} "{self.subject()}"'

    @property
    def short_sha(self):
        return self.sha[0:10]

    def filtered_changed_files(self):
        return [
            'third_party/blink/web_tests/external/wpt/one.html',
            'third_party/blink/web_tests/external/wpt/two.html',
        ]

    def url(self):
        return 'https://fake-chromium-commit-viewer.org/+/%s' % self.short_sha

    def author(self):
        return self._author

    def subject(self):
        return self._subject

    def body(self):
        # The final newline is intentionally added to match the real behavior.
        if not self.change_id():
            return self._body + '\n'
        return self._body + '\n\nChange-Id: ' + self.change_id() + '\n'

    def message(self):
        return self.subject() + '\n\n' + self.body()

    def format_patch(self):
        return self._patch

    def change_id(self):
        return self._change_id
