(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startBlank('Verifies that Overlay.setShowWindowControlsOverlay works.');

    await dp.DOM.enable();
    await dp.Overlay.enable();

    await dp.Overlay.setInspectMode({
      mode: 'searchForNode',
      highlightConfig: {},
    });

    async function waitForAnimationFrame() {
      await session.evaluateAsync(() => {
        return new Promise(resolve => requestAnimationFrame(resolve));
      });
    }

    async function takeViewportResetCommands(count) {
      return await session.evaluate((count) => {
        return internals.evaluateInInspectorOverlay(`(function () {
          const commands = window.commands;
          window.commands = [];
          return JSON.stringify(commands.slice(0, ${count}), null, 2);
        })()`);
      }, count);
    }

    async function runTest(label, config, commandCount) {
      // Consume another frame and delete any commands that might come from
      // a previous test.
      await waitForAnimationFrame();
      await takeViewportResetCommands(0);
      const result = await dp.Overlay.setShowWindowControlsOverlay(config);

      await waitForAnimationFrame();
      testRunner.log('Response to setShowWindowControlsOverlay:');
      testRunner.log(result);
      testRunner.log(label);
      testRunner.log(await takeViewportResetCommands(commandCount));
    }

    await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        window.commands = [];
        window.dispatch = ([name, data]) => {
          window.commands.push({name, data});
        };
      })()`);
    });

    await runTest('Window Controls Overlay Mixed Configuration', {
      windowControlsOverlayConfig: {showCSS: true, selectedPlatform: 'Linux', themeColor: '#FFA123'}
    }, 3);

    await runTest('Window Controls Overlay Empty Configuration', {}, 1);

    testRunner.completeTest();
  })
