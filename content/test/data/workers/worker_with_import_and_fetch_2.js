// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The service/shared worker for the network isolation key test.
const url_params = new URLSearchParams(location.search);
let import_script_url = url_params.get("import_script_url");
let fetch_url = url_params.get("fetch_url");
importScripts(import_script_url);
fetch(fetch_url);
