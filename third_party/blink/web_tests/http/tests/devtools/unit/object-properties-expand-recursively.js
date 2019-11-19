// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that ObjectPropertiesSection expands recursively.\n`);
  await TestRunner.loadModule('object_ui');

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

  var localObject = SDK.RemoteObject.fromLocalObject(object);
  var propertiesSection = new ObjectUI.ObjectPropertiesSection(localObject, 'JSON');
  await propertiesSection.objectTreeElement().expandRecursively();

  TestRunner.addResult(TestRunner.textContentWithLineBreaks(propertiesSection.element));
  TestRunner.completeTest();
})();
