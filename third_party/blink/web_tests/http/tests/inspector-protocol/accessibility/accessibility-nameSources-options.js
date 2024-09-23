(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
     <style>
     body.done .tests {
       display: none;
     }
     </style>
     <script>
       function done() {
           document.body.classList.add('done');
       }
     </script>
     <div class='tests'>

     <select>
       <option id="option1" aria-label="label" value="foo">x</option>
     </select>

     <select>
       <option id="option2" value="foo">x</option>
     </select>

     <select>
       <option id="option3" aria-label="label">x</option>
     </select>

     <select>
       <option id="option4">x</option>
     </select>
     </div>
  `, 'Tests name sources in <option>s.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('option', false, msg);
})
