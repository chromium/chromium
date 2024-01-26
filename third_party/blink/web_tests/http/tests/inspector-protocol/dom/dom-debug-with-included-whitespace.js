(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The test page contains a DIV with one child node
  // The function "addWhitespaceNode" inserts a whitespace node as first child which has a DOMNodeRemoved event listener attached.
  const {dp} = await testRunner.startHTML(`<html><div id=outer><span id=inner>Inner</span></div><script>
  function addWhitespaceNode() {
    const div = document.getElementById("outer");
    const node = document.createTextNode(" ");
    node.addEventListener('DOMNodeRemoved', function(){}, false);
    div.insertBefore(node, div.firstChild);
  }
  function removeFirstChild() {
    const div = document.getElementById("outer");
    div.removeChild(div.firstChild);
  }</script>
  </html>`, 'Tests that DOM debugging with DOM.enable(includeWhitespace:"all") works');

  async function testDomDebuggingWithWhitespaceText(includeWhitespace) {
    let result;

    testRunner.log("Testing with includeWhitespace: " + includeWhitespace)

    // add a whitespace text node as firstChild of DIV:
    await dp.Runtime.evaluate({expression:"addWhitespaceNode()"})
    await dp.DOM.enable({includeWhitespace});
    await dp.Debugger.enable();

    result = (await dp.DOM.getDocument({depth:-1})).result;

    const root = result.root;
    const div = root.children[0].children[1].children[0];
    const firstChild = div.children[0];
    testRunner.log("  FirstChild tag name: " + firstChild.nodeName)

    // check that the event listener on the whitespace node can be retreived
    result = (await dp.DOM.resolveNode({nodeId:div.nodeId})).result;
    const objectId = result.object.objectId;

    // get all listeners on the outer div, including all listeners on child nodes
    result = (await dp.DOMDebugger.getEventListeners({objectId,depth:-1})).result;
    const listeners = result.listeners;

    testRunner.log("  Listeners: " + listeners.length);
    for (const listener of listeners) {
            testRunner.log("    " + listener.type);
    }

    // check that the DOMBreakpoint is triggered
    const nodeId = firstChild.nodeId;
    await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: "node-removed"});

    dp.Debugger.oncePaused().then(function(messageObject) {
      testRunner.log("  Debugger paused on " + messageObject.params.data.type);

      // check if nodeId is correct:
      const isFirstChild = (messageObject.params.data.nodeId == nodeId);
      testRunner.log("    nodeId is " + (isFirstChild ? "" : "not ") + "correct");

      dp.Debugger.resume();
    });

    testRunner.log("  Removing first child...");
    result = (await dp.Runtime.evaluate({expression: "removeFirstChild()"}));
    testRunner.log("  Done removing first child.");

    await dp.DOM.disable();
  };

  // With includeWhitespace:"all", event listeners are reported and it is possible to register a DOMBreakpoint
  await testDomDebuggingWithWhitespaceText("all");

  // With includeWhitespace:"none" (default), event listeners on whitespace nodes are ignored
  await testDomDebuggingWithWhitespaceText("none");

  testRunner.completeTest();
})
