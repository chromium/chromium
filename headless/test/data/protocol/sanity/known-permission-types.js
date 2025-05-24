// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank('Tests all known permission types.');

  async function grant(permission) {
    const result =
        await dp.Browser.grantPermissions({permissions: [permission]});
    if (result.error) {
      testRunner.log(
          `Failed to grant '${permission}', error: ${result.error.message}`);
    } else {
      testRunner.log(`Granted '${permission}'`);
    }
  }

  await dp.Browser.resetPermissions();

  const permissions = testRunner.params('permissions')
  for (const permission of permissions) {
    await grant(permission);
  }

  testRunner.completeTest();
})
