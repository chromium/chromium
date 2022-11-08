// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const url_params = new URLSearchParams(location.search);
let fetch_url = url_params.get("fetch_url");
fetch(fetch_url, {browsingTopics: true});
