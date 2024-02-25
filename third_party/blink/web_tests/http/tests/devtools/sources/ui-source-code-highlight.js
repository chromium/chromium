
import {TestRunner} from 'test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';
(async function() {
  TestRunner.addResult(`Tests that network-loaded UISourceCodes are highlighted according to their HTTP header`);
  TestRunner.addResult(`mime type instead of their extension. crbug.com/411863\n`);

  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('resources/ui-source-code-highlight.php');

  var uiSourceCodes = Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodes();

  for (var i = 0; i < uiSourceCodes.length; ++i) {
    var uiSourceCode = uiSourceCodes[i];
    if (!/.php$/.test(uiSourceCode.url()))
      continue;
    if (uiSourceCode.project().type() !== Workspace.Workspace.projectTypes.Network)
      continue;
    TestRunner.addResult('Highlight mimeType: ' + uiSourceCode.mimeType());
    TestRunner.completeTest();
    return;
  }

  TestRunner.addResult('Failed to find source code with .php extension.');
  TestRunner.completeTest();
})();
