// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

function messageHost(messageName: string, payload: Object): void {
  const message = {'command': messageName, 'payload': payload};
  sendWebKitMessage('CWVWebViewMessage', message);
}

gCrWeb.cwvMessaging = {messageHost};
