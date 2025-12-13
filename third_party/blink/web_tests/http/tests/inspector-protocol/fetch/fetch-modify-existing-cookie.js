(async function (testRunner) {
    const { session, dp } = await testRunner.startBlank(
        'Tests that Fetch.continueRequest can override a cookie on main resource navigations and fetch requests, including redirects.');

    const sameOriginCookieToSet = 'original_cookie=from_server; Path=/';
    const sameOriginSetCookieUrl = `http://127.0.0.1:8000/inspector-protocol/network/resources/set-cookie.php?cookie=${encodeURIComponent(sameOriginCookieToSet)}`;
    const sameOriginEchoUrl = 'http://127.0.0.1:8000/inspector-protocol/fetch/resources/cors-echo-headers.php?headers=HTTP_COOKIE';
    const sameOriginRedirectUrl = `http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${sameOriginEchoUrl}`;

    const crossOriginCookieToSet = 'original_cross_origin_cookie=from_server; Path=/';
    const crossOriginSetCookieUrl = `http://localhost:8000/inspector-protocol/network/resources/set-cookie.php?cookie=${encodeURIComponent(crossOriginCookieToSet)}`;
    const crossOriginEchoUrl = `http://localhost:8000/inspector-protocol/fetch/resources/cors-echo-headers.php?headers=HTTP_COOKIE`;
    const crossOriginRedirectUrl = `http://127.0.0.1:8000/inspector-protocol/fetch/resources/redirect.pl?${crossOriginEchoUrl}`;

    await dp.Fetch.enable();
    await dp.Network.enable();

    async function setCookieForDomain(url) {
        const domain = new URL(url).hostname;

        const listener = event => {
            const { requestId, resourceType, request } = event.params;
            if (resourceType === 'Document' && request.url === url) {
                testRunner.log(`Intercepting navigation to set cookie for ${domain}...`);
            }
            dp.Fetch.continueRequest({ requestId });
        };

        testRunner.log(`\n--- Setting baseline cookie for ${domain} ---`);
        dp.Fetch.onRequestPaused(listener);
        dp.Page.navigate({ url });
        await dp.Network.onceLoadingFinished();
        dp.Fetch.offRequestPaused(listener);
        testRunner.log(`Cookie for ${domain} should now be set.`);
    }

    async function testNavigationCookieOverride({
        testName,
        urlToNavigate,
        urlToIntercept,
        isRedirect = false,
        cookieToSet
    }) {
        testRunner.log(`\n--- Running Navigation Test: ${testName} ---`);

        const listener = event => {
            const { requestId, resourceType, request } = event.params;
            if (resourceType === 'Document') {
                if (request.url === urlToNavigate && isRedirect) {
                    testRunner.log(`Intercepting initial navigation to redirect script: ${urlToNavigate}`);
                    dp.Fetch.continueRequest({ requestId });
                    testRunner.log('Continuing initial navigation, awaiting redirect...');
                    return;
                } else if (request.url === urlToIntercept) {
                    testRunner.log(`Intercepting final navigation to: ${urlToIntercept}`);
                    testRunner.log(`Attempting to override Cookie header to: "${cookieToSet}"`);
                    dp.Fetch.continueRequest({
                        requestId,
                        headers: [{ name: 'Cookie', value: cookieToSet }]
                    });
                    return;
                }
            }
            dp.Fetch.continueRequest({ requestId });
        };

        dp.Fetch.onRequestPaused(listener);
        dp.Page.navigate({ url: urlToNavigate });
        await dp.Network.onceLoadingFinished();
        dp.Fetch.offRequestPaused(listener);

        testRunner.log(`Navigation for ${testName} complete.`);
        const result = await session.evaluate('document.body.innerText');
        testRunner.log(`[Verification] Server response for ${testName}: ${result.trim()}`);
    }

    async function testFetchCookieOverride({
        testName,
        urlToFetch,
        urlToIntercept,
        isRedirect = false,
        cookieToSet
    }) {
        testRunner.log(`\n--- Running fetch() Test: ${testName} ---`);

        const listener = event => {
            const { requestId, request } = event.params;
            if (request.url === urlToFetch && isRedirect) {
                testRunner.log(`Intercepting initial fetch to redirect script: ${urlToFetch}`);
                dp.Fetch.continueRequest({ requestId: requestId });
                testRunner.log('Continuing initial fetch, awaiting redirect...');
                return;
            } else if (request.url === urlToIntercept) {
                testRunner.log(`Intercepting final fetch to: ${urlToIntercept}`);
                testRunner.log(`Attempting to override Cookie header to: "${cookieToSet}"`);
                dp.Fetch.continueRequest({
                    requestId: requestId,
                    headers: [{ name: 'Cookie', value: cookieToSet }]
                });
                return;
            }
            dp.Fetch.continueRequest({ requestId });
        };

        dp.Fetch.onRequestPaused(listener);
        const fetchResult = await session.evaluateAsync(`fetch('${urlToFetch}').then(res => res.text())`);
        dp.Fetch.offRequestPaused(listener);

        testRunner.log(`fetch() for ${testName} complete.`);
        testRunner.log(`[Verification] Server response for ${testName}: ${fetchResult.trim()}`);
    }

    await setCookieForDomain(crossOriginSetCookieUrl);
    await setCookieForDomain(sameOriginSetCookieUrl);

    await testFetchCookieOverride({
        testName: 'Direct Fetch',
        urlToFetch: sameOriginEchoUrl,
        urlToIntercept: sameOriginEchoUrl,
        cookieToSet: 'modified_cookie_for_direct_fetch=from_devtools'
    });

    await testFetchCookieOverride({
        testName: 'Same-Origin Redirect Fetch',
        urlToFetch: sameOriginRedirectUrl,
        urlToIntercept: sameOriginEchoUrl,
        isRedirect: true,
        cookieToSet: 'modified_cookie_for_redirect_fetch=from_devtools'
    });

    await testFetchCookieOverride({
        testName: 'Cross-Origin Redirect Fetch',
        urlToFetch: crossOriginRedirectUrl,
        urlToIntercept: crossOriginEchoUrl,
        isRedirect: true,
        cookieToSet: 'modified_cookie_for_cors_fetch=from_devtools'
    });

    await testNavigationCookieOverride({
        testName: 'Direct Navigation',
        urlToNavigate: sameOriginEchoUrl,
        urlToIntercept: sameOriginEchoUrl,
        cookieToSet: 'modified_cookie_for_direct_nav=from_devtools'
    });

    await testNavigationCookieOverride({
        testName: 'Same-Origin Redirect Navigation',
        urlToNavigate: sameOriginRedirectUrl,
        urlToIntercept: sameOriginEchoUrl,
        isRedirect: true,
        cookieToSet: 'modified_cookie_for_redirect_nav=from_devtools'
    });

    await testNavigationCookieOverride({
        testName: 'Cross-Origin Redirect Navigation',
        urlToNavigate: crossOriginRedirectUrl,
        urlToIntercept: crossOriginEchoUrl,
        isRedirect: true,
        cookieToSet: 'modified_cookie_for_cors_nav=from_devtools'
    });

    testRunner.completeTest();
})