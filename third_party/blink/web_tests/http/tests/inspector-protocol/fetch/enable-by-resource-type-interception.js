(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { session, dp } = await testRunner.startBlank(
        `Test Fetch.enable successfully intercepts various resource types.`);

    const testCases = [
        {
            name: 'Document',
            resourceType: 'Document',
            triggerAction: () => {
                const iframe = document.createElement('iframe');
                iframe.src = 'http://127.0.0.1:8000/inspector-protocol/resources/empty.html';
                document.body.appendChild(iframe);
            }
        },
        {
            name: 'Stylesheet',
            resourceType: 'Stylesheet',
            triggerAction: () => {
                const link = document.createElement('link');
                link.rel = 'stylesheet';
                link.type = 'text/css';
                link.href = 'http://127.0.0.1:8000/inspector-protocol/resources/css-module.css';
                document.head.appendChild(link);
            }
        },
        {
            name: 'Image',
            resourceType: 'Image',
            triggerAction: () => {
                const img = new Image();
                img.src = 'http://127.0.0.1:8000/inspector-protocol/resources/image.png';
            }
        },
        {
            name: 'Media',
            resourceType: 'Media',
            triggerAction: () => {
                const video = document.createElement('video');
                video.src = 'http://127.0.0.1:8000/misc/resources/empty.ogv';
                document.body.appendChild(video);
            }
        },
        {
            name: 'Font',
            resourceType: 'Font',
            triggerAction: () => {
                const style = document.createElement('style');
                style.textContent = `@font-face { font-family: 'testfont'; src: url('http://127.0.0.1:8000/security/resources/montez.woff2'); }`;
                document.head.appendChild(style);
                const div = document.createElement('div');
                div.style.fontFamily = 'testfont';
                div.textContent = 'test';
                document.body.appendChild(div);
            }
        },
        {
            name: 'Script',
            resourceType: 'Script',
            triggerAction: () => {
                const script = document.createElement('script');
                script.src = 'http://127.0.0.1:8000/inspector-protocol/resources/blank.js';
                document.body.appendChild(script);
            }
        },
        {
            name: 'XHR',
            resourceType: 'XHR',
            triggerAction: () => {
                const xhr = new XMLHttpRequest();
                xhr.open('GET', 'http://127.0.0.1:8000/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_USER_AGENT', true);
                xhr.send();
            }
        },
        {
            name: 'Fetch',
            resourceType: 'Fetch',
            triggerAction: () => {
                fetch('http://127.0.0.1:8000/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_USER_AGENT');
            }
        },
        {
            name: 'EventSource',
            resourceType: 'EventSource',
            triggerAction: () => {
                new EventSource('http://127.0.0.1:8000/eventsource/resources/event-stream.php');
            }
        },
        {
            name: 'Ping',
            resourceType: 'Ping',
            triggerAction: () => {
                navigator.sendBeacon('http://127.0.0.1:8000/inspector-protocol/fetch/resources/post-echo.pl', 'ping');
            }
        },
        {
            name: 'Other (Worker)',
            resourceType: 'Other',
            triggerAction: () => {
                new Worker('http://127.0.0.1:8000/inspector-protocol/resources/blank.js');
            }
        },
        {
            name: 'Other (SharedWorker)',
            resourceType: 'Other',
            triggerAction: () => {
                new SharedWorker('http://127.0.0.1:8000/inspector-protocol/resources/blank.js');
            }
        },
    ];

    for (const testCase of testCases) {
        testRunner.log(`\n--- Running test: [${testCase.name}] with resourceType: [${testCase.resourceType}] ---`);
        await runTest(testCase);
    }

    testRunner.completeTest();

    async function runTest(testCase) {
        const enableResponse = await dp.Fetch.enable({ patterns: [{ resourceType: testCase.resourceType }] });
        if (enableResponse.error) {
            testRunner.log(`FAILURE: Fetch.enable failed unexpectedly: ${enableResponse.error.message}`);
            return;
        }

        const requestPausedPromise = dp.Fetch.onceRequestPaused();

        session.evaluate(`(${testCase.triggerAction.toString()})()`);

        const result = await requestPausedPromise;
        const resourceType = result.params.resourceType;
        testRunner.log(`SUCCESS: Intercepted request with correct resource type: ${resourceType}`);
        await dp.Fetch.continueRequest({ requestId: result.params.requestId });

        await dp.Fetch.disable();
    }
})