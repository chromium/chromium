// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {isTextField, removeQueryAndReferenceFromURL, sendWebKitMessage, trim} from '//ios/web/public/js_messaging/resources/utils.js';

const utils_tests = new CrWebApi();

gCrWeb.registerApi('utils_tests', utils_tests);

utils_tests.addFunction(
    'removeQueryAndReferenceFromURL', removeQueryAndReferenceFromURL);
utils_tests.addFunction('sendWebKitMessage', sendWebKitMessage);
utils_tests.addFunction('trim', trim);
utils_tests.addFunction('isTextField', isTextField);
