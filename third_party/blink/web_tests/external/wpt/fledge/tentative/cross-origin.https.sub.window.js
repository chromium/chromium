// META: script=/resources/testdriver.js
// META: script=/common/utils.js
// META: script=resources/fledge-util.js
// META: timeout=long

"use strict;"

const OTHER_ORIGIN1 = 'https://{{hosts[alt][]}}:{{ports[https][0]}}';

function runInIframe(test, iframe, script) {
  const messageUuid = generateUuid(test);

  return new Promise(function(resolve, reject) {
    function WaitForMessage(event) {
      if (event.data.messageUuid != messageUuid)
        return;
      if (event.data.result === 'success') {
        resolve();
      } else {
        reject(event.data.result);
      }
    }
    window.addEventListener('message', WaitForMessage);
    iframe.contentWindow.postMessage({messageUuid: messageUuid, script: script}, '*');
  });
}

// Creates an iframe and navigates to a URL on "origin", and waits for the URL
// to finish loading by waiting for the iframe to send an event. Then returns
// the iframe.
//
// Also adds a cleanup callback to "test", which runs all cleanup functions
// added to the iframe and waits for them to complete, and then destroys the
// iframe.
async function createIframe(test, origin, permissions) {
  const iframeUuid = generateUuid(test);
  const iframeUrl = `${origin}${RESOURCE_PATH}iframe.sub.html?uuid=${iframeUuid}`;
  let iframe = document.createElement('iframe');
  await new Promise(function(resolve, reject) {
    function WaitForMessage(event) {
      if (event.data.messageUuid != iframeUuid)
        return;
      if (event.data.result === 'load complete') {
        resolve();
      } else {
        reject(event.data.result);
      }
    }
    window.addEventListener('message', WaitForMessage);
    if (permissions)
      iframe.allow = permissions;
    iframe.src = iframeUrl;
    document.body.appendChild(iframe);

    test.add_cleanup(async () => {
      await runInIframe(test, iframe, "await test_instance.do_cleanup();");
      document.body.removeChild(iframe);
    });
  });
  return iframe;
}

////////////////////////////////////////////////////////////////////////////////
// Join interest group in iframe tests.
////////////////////////////////////////////////////////////////////////////////

promise_test(async test => {
  const uuid = generateUuid(test);
  let iframe = await createIframe(test, document.location.origin);

  // Join a same-origin InterestGroup in a iframe navigated to its origin.
  await runInIframe(test, iframe, `await joinInterestGroup(test_instance, "${uuid}");`);

  // Run an auction using window.location.origin as a bidder. The IG should
  // make a bid and win an auction.
  let config = await runBasicFledgeTestExpectingWinner(test, uuid);
}, 'Join interest group in same-origin iframe, default permissions.');

promise_test(async test => {
  const uuid = generateUuid(test);
  let iframe = await createIframe(test, OTHER_ORIGIN1);

  // Join a cross-origin InterestGroup in a iframe navigated to its origin.
  await runInIframe(test, iframe, `await joinInterestGroup(test_instance, "${uuid}");`);

  // Run an auction in this frame using the other origin as a bidder. The IG should
  // make a bid and win an auction.
  //
  // TODO: Once the permission defaults to not being able to join InterestGroups in
  // cross-origin iframes, this auction should have no winner.
  let config = await runBasicFledgeTestExpectingWinner(
      test, uuid,
      { interestGroupBuyers: [OTHER_ORIGIN1],
        scoreAd: `if (browserSignals.interestGroupOwner !== "${OTHER_ORIGIN1}")
                    throw "Wrong owner: " + browserSignals.interestGroupOwner`
      });
}, 'Join interest group in cross-origin iframe, default permissions.');

promise_test(async test => {
  const uuid = generateUuid(test);
  let iframe = await createIframe(test, OTHER_ORIGIN1, 'join-ad-interest-group');

  // Join a cross-origin InterestGroup in a iframe navigated to its origin.
  await runInIframe(test, iframe, `await joinInterestGroup(test_instance, "${uuid}");`);

  // Run an auction in this frame using the other origin as a bidder. The IG should
  // make a bid and win an auction.
  let config = await runBasicFledgeTestExpectingWinner(
      test, uuid,
      { interestGroupBuyers: [OTHER_ORIGIN1],
        scoreAd: `if (browserSignals.interestGroupOwner !== "${OTHER_ORIGIN1}")
                    throw "Wrong owner: " + browserSignals.interestGroupOwner`
      });
}, 'Join interest group in cross-origin iframe with join-ad-interest-group permission.');

promise_test(async test => {
  const uuid = generateUuid(test);
  let iframe = await createIframe(test, OTHER_ORIGIN1, "join-ad-interest-group 'none'");

  // Try to join an InterestGroup in a cross-origin iframe whose permissions policy
  // blocks joining interest groups. An exception should be thrown, and the interest
  // group should not be joined.
  await runInIframe(test, iframe,
                    `try {
                       await joinInterestGroup(test_instance, "${uuid}");
                     } catch (e) {
                       return "success";
                     }
                     return "exception unexpectedly not thrown";`);

  // Run an auction in this frame using the other origin as a bidder. Since the join
  // should have failed, the auction should have no winner.
  let config = await runBasicFledgeTestExpectingNoWinner(
      test, uuid,
      { interestGroupBuyers: [OTHER_ORIGIN1] });
}, 'Join interest group in cross-origin iframe with join-ad-interest-group permission denied.');

promise_test(async test => {
  const uuid = generateUuid(test);
  let iframe = await createIframe(test, OTHER_ORIGIN1, 'join-ad-interest-group');

  // Try to join an IG with the parent's origin as an owner in a cross-origin iframe.
  // This should require a .well-known fetch to the parents origin, which will not
  // grant permission. The case where permission is granted is not yet testable.
  let interestGroup = JSON.stringify(createInterestGroupForOrigin(uuid, window.location.origin));
  await runInIframe(test, iframe,
                    `joinInterestGroup(test_instance, "${uuid}", ${interestGroup})`);

  // Run an auction with this page's origin as a bidder. Since the join
  // should have failed, the auction should have no winner.
  let config = await runBasicFledgeTestExpectingNoWinner(test, uuid);
}, "Join interest group owned by parent's origin in cross-origin iframe.");
