(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { dp } = await testRunner.startBlank(
        `Test Fetch.enable input validation for unsupported resource types.`);

    const testCases = [
        'TextTrack',
        'Prefetch',
        'WebSocket',
        'Manifest',
        'SignedExchange',
        'Preflight',
        'FedCM',
    ];

    for (const testCase of testCases) {
        await runValidationErrorTest(testCase);
    }

    testRunner.completeTest();

    async function runValidationErrorTest(testCase) {
        testRunner.log(`\n--- Running test: [${testCase}] ---`);
        const response = await dp.Fetch.enable({ patterns: [{ resourceType: testCase }] });

        if (response.error) {
            testRunner.log(response.error.message);
        } else {
            testRunner.log(response);
        }
        // No need to disable, as it failed to enable.
    }
})