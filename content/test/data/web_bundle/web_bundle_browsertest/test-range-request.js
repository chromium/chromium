// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
    let resp = await fetch("/hello.txt", {headers: {'range': 'bytes=7-17'}});
    let text = await resp.text();
    const expected = 'Web Bundles';
    if (text !== expected) {
        return fail(`expected "${expected}", but got "${text}"`);
    }

    try {
        await fetch("/hello.txt", {headers: {'range': 'bytes=10000-10001'}});
        return fail('Out-of-range range request should fail');
    } catch(err) {
        success();
    }
})();
