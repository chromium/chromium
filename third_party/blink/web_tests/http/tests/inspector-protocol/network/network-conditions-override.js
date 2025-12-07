(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { session, dp } = await testRunner.startBlank(
        'Tests that Network emulation commands correctly handle various override scenarios.');

    await dp.Network.enable();

    async function getNavigatorState() {
        const { result: { result: { value: state } } } = await dp.Runtime.evaluate({
            expression: `(function() {
        const conn = navigator.connection || {};
        return {
          onLine: navigator.onLine,
          connection: {
            type: conn.type,
            effectiveType: conn.effectiveType,
            downlink: conn.downlink,
            rtt: conn.rtt,
          }
        };
      })()`,
            returnByValue: true
        });
        return state;
    }
    async function runTestForCommand(commandName) {
        testRunner.log(`\n\n--- Testing command: Network.${commandName} ---`);

        testRunner.log('Capturing initial state...');
        const initialState = await getNavigatorState();

        testRunner.log('\nApplying full override with valid values...');
        // For an input of 500 KB/s, due to GetRandomMultiplier, the theoretical max output is 4.4 Mbps.
        // We will use a safe upper bound of 5 for the test.
        const fullOverrideThroughputMbps = 5;
        await dp.Network[commandName]({
            offline: false,
            latency: 2500,
            downloadThroughput: 500 * 1024,
            uploadThroughput: 500 * 1024,
        });
        const fullOverrideState = await getNavigatorState();

        testRunner.log('Verifying state after full override:');
        const rtt1 = fullOverrideState.connection.rtt;
        // RTT is randomized (0.9x-1.1x) and rounded to the nearest 50ms.
        // For latency=2500, the range is [2250, 2750].
        testRunner.log(`  rtt is within expected range [2000, 3000]? ` + (rtt1 >= 2000 && rtt1 <= 3000));
        testRunner.log(`  effectiveType is ${fullOverrideState.connection.effectiveType}`);
        testRunner.log(`  downlink is constrained (<= ${fullOverrideThroughputMbps})? ` +
            (fullOverrideState.connection.downlink <= fullOverrideThroughputMbps));


        testRunner.log('\nApplying partial override with downloadThroughput = -1...');
        const partialOverrideParams1 = {
            offline: false,
            latency: 200,
            downloadThroughput: -1, // <= 0, should disable downlink throttling.
            uploadThroughput: 500 * 1024,
        };
        await dp.Network[commandName](partialOverrideParams1);

        const partialOverrideState1 = await getNavigatorState();
        testRunner.log('Verifying state after partial override with -1:');
        // For latency=200, the randomized range [180, 220] always rounds to 200.
        testRunner.log(`  rtt is ${partialOverrideState1.connection.rtt}`);
        testRunner.log(`  effectiveType is ${partialOverrideState1.connection.effectiveType}`);
        testRunner.log(`  downlink is un-constrained (> ${fullOverrideThroughputMbps})? ` +
            (partialOverrideState1.connection.downlink > fullOverrideThroughputMbps));


        testRunner.log('\nApplying partial override with downloadThroughput = 0...');
        const partialOverrideParams2 = {
            offline: false,
            latency: 300, // Use a different latency to prove this is a new state.
            downloadThroughput: 0, // <= 0, should also disable downlink throttling.
            uploadThroughput: 500 * 1024,
        };
        await dp.Network[commandName](partialOverrideParams2);

        const partialOverrideState2 = await getNavigatorState();
        testRunner.log('Verifying state after partial override with 0:');
        const rtt2 = partialOverrideState2.connection.rtt;
        // RTT is randomized (0.9x-1.1x) and rounded to the nearest 50ms.
        // For latency=300, the range is [250, 350].
        testRunner.log(`  rtt is within expected range [200, 400]? ` + (rtt2 >= 200 && rtt2 <= 400));
        testRunner.log(`  effectiveType is ${partialOverrideState2.connection.effectiveType}`);
        testRunner.log(`  downlink is ${partialOverrideState2.connection.downlink}`);


        testRunner.log('\nDisabling all overrides to reset to initial state...');
        await dp.Network[commandName]({
            offline: false,
            latency: -1,
            downloadThroughput: -1,
            uploadThroughput: -1,
        });
        const finalState = await getNavigatorState();

        const wasOnLineRestored = initialState.onLine === finalState.onLine;
        const wasTypeRestored = initialState.connection.type === finalState.connection.type;
        const wasEffectiveTypeRestored = initialState.connection.effectiveType === finalState.connection.effectiveType;

        // For RTT, the tolerance must be relative to the initial value to account for
        // +/-10% randomization on both initial and final reads, plus rounding error.
        const rttDifference = Math.abs(initialState.connection.rtt - finalState.connection.rtt);
        // A safe tolerance: 25% of the value for randomization range + 50ms for rounding error.
        const rttTolerance = initialState.connection.rtt * 0.25 + 50;
        const wasRttRestored = rttDifference <= rttTolerance;

        // The same logic applies to downlink.
        const downlinkDifference = Math.abs(initialState.connection.downlink - finalState.connection.downlink);
        // A safe tolerance: 25% of the value for randomization range + 0.05 Mbps for rounding error.
        const downlinkTolerance = initialState.connection.downlink * 0.25 + 0.05;
        const wasDownlinkRestored = downlinkDifference <= downlinkTolerance;

        const wasStateFullyRestored = wasOnLineRestored && wasTypeRestored && wasEffectiveTypeRestored && wasRttRestored && wasDownlinkRestored;
        testRunner.log('\nFinal state matches initial state? ' + wasStateFullyRestored);

    }

    // Run the entire test suite for both CDP commands.
    await runTestForCommand('emulateNetworkConditions');
    await runTestForCommand('overrideNetworkState');

    testRunner.completeTest();
})
