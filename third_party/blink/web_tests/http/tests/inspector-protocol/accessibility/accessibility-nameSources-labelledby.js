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
    <!-- Compare with accessibility/name-calc-figure.html, accessibility/name-calc-img.html, accessibility/name-calc-presentational.html and accessibility/name-calc-svg.html-->
    <div class='tests'>
      <div data-dump id='div'>Div Contents</div>

      <button data-dump id='self'>Contents of button</button>

      <button data-dump id='labelledby' aria-labelledby='label1'>Contents</button>
      <div id='label1'>Label 1</div>

      <button data-dump id='labelledbySelf' aria-labelledby='labelledbySelf'>Contents</button>

      <button data-dump id='labelledby3' aria-labelledby='labelledby3 label3'>Contents</button>
      <div id='label3'>Label 3</div>

      <button data-dump id='labelledby4' aria-labelledby='label4'>Contents</button>
      <div id='label4' aria-labelledby='label4chained'>Contents 4</div>
      <p id='label4chained'>Contents 4 chained</p>

      <button data-dump id='labelledby5' aria-labelledby='label5'>Contents</button>

      <button data-dump id='labelledby6' aria-labelledby='label6'>Contents</button>
      <div id='label6'></div>

      <button data-dump id='labelledby7' aria-labelledby='label7'>Contents</button>
      <h3 id='label7' style='visibility: hidden'>Invisible label</h3>

      <button data-dump id='labelledby8' aria-labelledby='label8'>Contents</button>
      <h3 id='label8' style='display: none'>Display-none label</h3>

      <button data-dump id='labelOnly' aria-label='Label'>Contents</button>

      <button data-dump id='emptyLabel1' aria-label=''>Contents</button>

      <button data-dump id='emptyLabel2' aria-label>Contents</button>

      <button data-dump id='labelledby9' aria-labelledby='label9' aria-label='Label'>Contents</button>
      <div id='label9'>Labelledby 9</div>

      <button data-dump id='labelledby10' aria-labelledby='label10'>Contents</button>
      <div id='label10' aria-label='Label 10 label'>Contents 10</div>

      <button data-dump id='labelledby11' aria-labelledby='label11'>Contents</button>
      <div id='label11' aria-label=''>Contents 11</div>

      <button data-dump id='labelledby12' aria-labelledby='label12'>Contents</button>
      <div id='label12' aria-label='Label 12 label' aria-labelledby='label12chained'>Contents 12</div>
      <p id='label12chained'>Contents 12 chained</p>

      <input data-dump id='input1' aria-labelledby='list1'>
      <ul id='list1' aria-owns='list1_item3'>
          <li>A
          <li>B
      </ul>
      <div role='listitem' id='list1_item3'>C</div>
    </div>
  `, 'Tests name sources when used with aria-labelledby.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
