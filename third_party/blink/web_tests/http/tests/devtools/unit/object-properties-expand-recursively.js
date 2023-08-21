// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test that ObjectPropertiesSection expands recursively.\n`);
  await TestRunner.loadLegacyModule('ui/legacy/components/object_ui');

  var object = {
    "foo": {
      "bar": {
        "baz": {
          "quux": {
            "corge": "plugh"
          }
        }
      },

      "quuz": {
        "garply": "xyzzy",
        "thud": {
          "wibble": "wobble"
        }
      }
    }
  }

  var localObject = SDK.RemoteObject.RemoteObject.fromLocalObject(object);
  var propertiesSection = new ObjectUI.ObjectPropertiesSection(localObject, 'JSON');
  await propertiesSection.objectTreeElement().expandRecursively();

  TestRunner.addResult(TestRunner.textContentWithLineBreaks(propertiesSection.element));
  TestRunner.completeTest();
})();
