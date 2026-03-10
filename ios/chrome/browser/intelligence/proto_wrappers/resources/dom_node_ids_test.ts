// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getNodeById, getOrCreateNodeId} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Set up APIs and the JS test environment for testing
 * the Dom Node ID functionality.
 */

const api = new CrWebApi('dom_node_ids_test');
api.addFunction('getNodeById', getNodeById);
api.addFunction('getOrCreateNodeId', getOrCreateNodeId);

gCrWeb.registerApi(api);
