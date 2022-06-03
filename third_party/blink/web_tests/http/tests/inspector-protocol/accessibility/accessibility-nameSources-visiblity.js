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
    <!-- Compare with accessibility/name-calc-figure.html, accessibility/name-calc-img.html, accessibility/name-calc-presentational.html and accessibility/name-calc-svg.html-->
    <div class='tests'>
      <div data-dump id='link1' tabIndex=0 role='link'>
        <p>1</p>
        <table role="presentation">
          <tr><td>2</td></tr>
          <tr><td style='visibility: hidden'>3</td></tr>
          <tr><td style='display:none'>4</td></tr>
          <tr style='visibility: hidden'><td>5</td></tr>
          <tr style='display: none'><td>6</td></tr>
        </table>
        <p>7</p>
      </div>

      <input data-dump id='input2' aria-labelledby='label2'>
      <div id='label2'>
        <p>1</p>
        <table role="presentation">
          <tr><td>2</td></tr>
          <tr><td style='visibility: hidden'>3</td></tr>
          <tr><td style='display:none'>4</td></tr>
          <tr style='visibility: hidden'><td>5</td></tr>
          <tr style='display: none'><td>6</td></tr>
        </table>
        <p>7</p>
      </div>

      <input data-dump id='input3' aria-labelledby='3a 3b 3c 3d 3e 3f 3g'>
      <p id='3a'>1</p>
      <table role="presentation">
        <tr><td id='3b'>2</td></tr>
        <tr><td id='3c' style='visibility: hidden'>3</td></tr>
        <tr><td id='3d' style='display:none'>4</td></tr>
        <tr id='3e' style='visibility: hidden'><td>5</td></tr>
        <tr id='3f' style='display: none'><td>6</td></tr>
      </table>
      <p id='3g'>7</p>

      <input data-dump id='input4' aria-labelledby='label4'>
      <div style='display: none'>
        <div id='label4'>
          <p>1</p>
          <table>
            <tr><td>2</td></tr>
            <tr><td style='visibility: hidden'>3</td></tr>
            <tr><td style='display:none'>4</td></tr>
            <tr style='visibility: hidden'><td>5</td></tr>
            <tr style='display: none'><td>6</td></tr>
          </table>
          <p>7</p>
        </div>
      </div>

      <h3 id='heading1'>
        Before
        <p id='hidden1' aria-hidden='true'>Hidden text</p>
        After
      </h3>
      <button data-dump id='button1' aria-labelledby='hidden1'></button>

      <h3 id='heading2'>
        Before
        <p id='hidden2' aria-hidden='true'>Hidden text</p>
        After
      </h3>
      <button data-dump id='button2' aria-labelledby='heading2'></button>

      <h3 id='heading3' aria-hidden='true'>
        Before
        <p id='hidden3'>Text within hidden subtree</p>
        After
      </h3>
      <button data-dump id='button3' aria-labelledby='hidden3'></button>

      <h3 id='heading4' aria-hidden='true'>
        Before
        <p id='hidden4' aria-hidden='true'>Text within hidden subtree</p>
        After
      </h3>
      <button data-dump id='button4' aria-labelledby='heading4'></button>

      <label for='input5' aria-hidden='true'>
        Before
        <p aria-hidden='true'>Hidden text</p>
        After</label>
      <input data-dump id='input5'>
    </div>
  `, 'Tests name sources in invisible nodes.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
