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
      <figure data-dump id='figure1'>
        <img src='resources/cake.png' alt='cake'>
      </figure>

      <figure data-dump id='figure2' title='figure2-title'>
        <img src='resources/cake.png' alt='cake'>
      </figure>


      <figure data-dump id='figure3' title='figure3-title'>
        <figcaption>figcaption3</figcaption>
        <img src='resources/cake.png' alt='cake'>
      </figure>

      <figure data-dump id='figure4' title='figure4-title' aria-label='figure4-aria-label'>
        <figcaption>figcaption4</figcaption>
        <img src='resources/cake.png' alt='cake'>
      </figure>

      <figure data-dump id='figure5' title='figure5-title' aria-label='figure5-aria-label' aria-labelledby='figure-labelledby5'>
        <figcaption>figcaption5</figcaption>
        <img src='resources/cake.png' alt='cake'>
      </figure>
      <span hidden='true' id='figure-labelledby5'>figure5-aria-labelledby</span>

      <img data-dump id='img1' src='resources/cake.png'>

      <img data-dump id='img2' title='img2-title' src='resources/cake.png'>

      <img data-dump id='img3' title='img3-title' alt='img3-alt' src='resources/cake.png'>

      <img data-dump id='img4' title='img4-title' alt='img4-alt' aria-label='img4-aria-label' src='resources/cake.png'>

      <img data-dump id='img5' title='img5-title' alt='img5-alt' aria-label='img5-aria-label' aria-labelledby='img-labelledby5' src='resources/cake.png'>
      <span hidden='true' id='img-labelledby5'>img5-aria-labelledby</span>

      <div data-dump tabIndex=0 role='link' id='link1'>
        I
        <img src='hidden.jpg' alt='do not' role='presentation' />
        like ice cream.
      </div>

      <svg data-dump id='svg1'>
        <title>svg1-title</title>
      </svg>
    </div>

    <img data-dump title="title" alt="" src='resources/cake.png'>
  `, 'Tests name sources in images and figures.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
