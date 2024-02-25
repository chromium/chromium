// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Import a module from a relative path, which will be normalized during
// importing time.
import {TestBindingInterface} from './test_api.test-mojom.m.js';

// Binding defined by the V8Environment.
atpconsole.log('Green is the loneliest color');

const remote = TestBindingInterface.getRemote();
remote.testComplete(/*success=*/ true);
