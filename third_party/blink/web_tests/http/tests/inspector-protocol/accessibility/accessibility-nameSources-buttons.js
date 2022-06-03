(async function(testRunner) {
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
     <!-- Compare with accessibility/name-calc-native-markup-buttons.html' -->
     <div class='tests'>
       <button id='button1'></button>

       <button id='button2'>button2-content</button>

       <button id='button3'><img src='resources/cake.png'></button>

       <button id='button4'><img src='resources/cake.png' alt='cake'></button>

       <button id='button5'>I love <img src='resources/cake.png'>!</button>

       <button id='button6'>I love <img src='resources/cake.png' alt='cake'>!</button>

       <button id='button7' title='button7-title'></button>

       <button id='button8' title='button8-title'>button8-content</button>

       <button id='button9' title='button9-title'><img src='resources/cake.png'></button>

       <button id='button10' title='button10-title'><img src='resources/cake.png' alt='cake'></button>

       <button id='button11'>button11-content</button>
       <label for='button11'>label-for-button11</label>
     </div>
  `, 'Tests name sources in buttons.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('button', false, msg);
})
