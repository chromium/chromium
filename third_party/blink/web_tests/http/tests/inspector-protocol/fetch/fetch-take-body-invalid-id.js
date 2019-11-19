(async function(testRunner) {
    var {page, session, dp} = await testRunner.startBlank(
        `Tests that Fetch.takeResponseBodyAsStream does not trigger a DCHECK when dispatching an error.`);

    await dp.Fetch.enable();
    const error = (await dp.Fetch.takeResponseBodyAsStream({requestId: "I'm not there"})).error;
    testRunner.log(`Error from Fetch.takeResponseBodyAsStream: ${JSON.stringify(error)}`);

    testRunner.completeTest();
})
