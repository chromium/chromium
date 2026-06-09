(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <div id="drop-target" style="width: 100px; height: 100px; background: blue"></div>
    <script>
      window.dropped_uri = null;
      const div = document.getElementById('drop-target');
      div.addEventListener('dragenter', e => e.preventDefault());
      div.addEventListener('dragover', e => e.preventDefault());
      div.addEventListener('drop', e => {
        e.preventDefault();
        window.dropped_uri = e.dataTransfer.getData('text/uri-list');
      });
    </script>
  `, `Tests that Input.dispatchDragEvent sanitizes URLs (FilterURL) when dropping onto a web page.`);

  const data = {
    items: [
      {
        mimeType: 'text/uri-list',
        data: 'file:///etc/passwd',
      }
    ],
    dragOperationsMask: 1,
  };

  await dp.Input.dispatchDragEvent({
    type: 'dragEnter',
    data,
    x: 50,
    y: 50,
  });

  await dp.Input.dispatchDragEvent({
    type: 'dragOver',
    data,
    x: 50,
    y: 50,
  });

  await dp.Input.dispatchDragEvent({
    type: 'drop',
    data,
    x: 50,
    y: 50,
  });

  const droppedUri = await session.evaluate(`window.dropped_uri`);
  testRunner.log('Dropped URI: ' + droppedUri);
  testRunner.completeTest();
})
