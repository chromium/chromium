// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that an attribution redirect with invalid trigger data triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/empty.html');

  const issuePromises = [dp.Audits.onceIssueAdded(), dp.Audits.onceIssueAdded()];
  await page.loadHTML(`
    <!DOCTYPE html>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect.php"></img>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect.php?trigger-data=badinteger"></img>`);

  const issues = (await Promise.all(issuePromises)).map(issue => issue.params.issue);

  // In rare cases the issues arrive in reverse order. Lets sort them to be sure.
  issues.sort((i1, i2) => {
    return (i1.invalidParamter < i2.invalidParamter) ? -1 :
           (i2.invalidParamter < i1.invalidParamter) ?  1 : 0;
  });

  testRunner.log(issues[0], "Issue reported: ", ['requestId']);
  testRunner.log(issues[1], "Issue reported: ", ['requestId']);

  testRunner.completeTest();
})
