// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {processChildFrameMessage} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {registerAllChildFrames} from '//components/autofill/ios/form_util/resources/child_frame_registration_test.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Set up APIs and the JS test environment for testing
 * PageContextWrapper features.
 */

const pageContextWrapperTestApi = new CrWebApi('pageContextWrapperTest');

pageContextWrapperTestApi.addFunction(
    'registerAllRemoteFrames', registerAllChildFrames);

gCrWeb.registerApi(pageContextWrapperTestApi);

// Listen to process registration messages to complete the remote=>local token
// registration.
window.addEventListener('message', processChildFrameMessage);
