// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BarCallbackRouter, BazCallbackRouter, FooRemote, TestWebUIJsBridge} from './web_ui_managed_interface_test.test-mojom-webui.js';

Object.assign(
    window,
    {BarCallbackRouter, BazCallbackRouter, FooRemote, TestWebUIJsBridge});
