const executor_path = "/common/dispatcher/executor.html?pipe=";
const coep_header = '|header(Cross-Origin-Embedder-Policy,require-corp)';

const isWPTSubEnabled = "{{GET[pipe]}}".includes("sub");

const getReportEndpointURL = (reportID) =>
  `/reporting/resources/report.py?reportID=${reportID}`;

const reportEndpoint = {
  name: "coop-report-endpoint",
  reportID: isWPTSubEnabled ? "{{GET[report_id]}}" : token(),
  reports: []
};
const reportOnlyEndpoint = {
  name: "coop-report-only-endpoint",
  reportID: isWPTSubEnabled ? "{{GET[report_only_id]}}" : token(),
  reports: []
};
const popupReportEndpoint = {
  name: "coop-popup-report-endpoint",
  reportID: token(),
  reports: []
};
const popupReportOnlyEndpoint = {
  name: "coop-popup-report-only-endpoint",
  reportID: token(),
  reports: []
};
const redirectReportEndpoint = {
  name: "coop-redirect-report-endpoint",
  reportID: token(),
  reports: []
};
const redirectReportOnlyEndpoint = {
  name: "coop-redirect-report-only-endpoint",
  reportID: token(),
  reports: []
};

const reportEndpoints = [
  reportEndpoint,
  reportOnlyEndpoint,
  popupReportEndpoint,
  popupReportOnlyEndpoint,
  redirectReportEndpoint,
  redirectReportOnlyEndpoint
];

// Allows RegExps to be pretty printed when printing unmatched expected reports.
Object.defineProperty(RegExp.prototype, "toJSON", {
  value: RegExp.prototype.toString
});

function wait(ms) {
  return new Promise(resolve => step_timeout(resolve, ms));
}

// Check whether a |report| is a "opener breakage" COOP report.
function isCoopOpenerBreakageReport(report) {
  if (report.type != "coop")
    return false;

  if (report.body.type != "navigation-from-response" &&
      report.body.type != "navigation-to-response") {
    return false;
  }

  return true;
}

async function clearReportsOnServer(host) {
  const res = await fetch(
    '/reporting/resources/report.py', {
      method: "POST",
    body: JSON.stringify({
      op: "DELETE",
      reportIDs: reportEndpoints.map(endpoint => endpoint.reportID)
    })
  });
  assert_equals(res.status, 200, "reports cleared");
}

async function pollReports(endpoint) {
  const res = await fetch(getReportEndpointURL(endpoint.reportID),
    { cache: 'no-store' });
  if (res.status !== 200) {
    return;
  }
  for (const report of await res.json()) {
    if (isCoopOpenerBreakageReport(report))
      endpoint.reports.push(report);
  }
}

// Recursively check that all members of expectedReport are present or matched
// in report.
// Report may have members not explicitly expected by expectedReport.
function isObjectAsExpected(report, expectedReport) {
  if (( report === undefined || report === null
        || expectedReport === undefined || expectedReport === null )
      && report !== expectedReport ) {
    return false;
  }
  if (expectedReport instanceof RegExp && typeof report === "string") {
    return expectedReport.test(report);
  }
  // Perform this check now, as RegExp and strings above have different typeof.
  if (typeof report !== typeof expectedReport)
    return false;
  if (typeof expectedReport === 'object') {
    return Object.keys(expectedReport).every(key => {
      return isObjectAsExpected(report[key], expectedReport[key]);
    });
  }
  return report == expectedReport;
}

async function checkForExpectedReport(expectedReport) {
  return new Promise( async (resolve, reject) => {
    const polls = 5;
    const waitTime = 200;
    for (var i=0; i < polls; ++i) {
      pollReports(expectedReport.endpoint);
      for (var j=0; j<expectedReport.endpoint.reports.length; ++j){
        if (isObjectAsExpected(expectedReport.endpoint.reports[j],
            expectedReport.report)){
          expectedReport.endpoint.reports.splice(j,1);
          resolve();
          return;
        }
      };
      await wait(waitTime);
    }
    reject(
      replaceTokensInReceivedReport(
        "No report matched the expected report for endpoint: "
        + expectedReport.endpoint.name
        + ", expected report: " + JSON.stringify(expectedReport.report)
        + ", within available reports: "
        + JSON.stringify(expectedReport.endpoint.reports)
    ));
  });
}

function replaceFromRegexOrString(str, match, value) {
  if (str instanceof RegExp) {
    return RegExp(str.source.replace(match, value));
  }
  return str.replace(match, value);
}

// Replace generated values in regexes and strings of an expected report:
// EXECUTOR_UUID: the uuid generated with token().
function replaceValuesInExpectedReport(expectedReport, executorUuid) {
  if (expectedReport.report.body !== undefined) {
    if (expectedReport.report.body.nextResponseURL !== undefined) {
      expectedReport.report.body.nextResponseURL = replaceFromRegexOrString(
          expectedReport.report.body.nextResponseURL, "EXECUTOR_UUID",
          executorUuid);
    }
    if (expectedReport.report.body.previousResponseURL !== undefined) {
      expectedReport.report.body.previousResponseURL = replaceFromRegexOrString(
          expectedReport.report.body.previousResponseURL, "EXECUTOR_UUID",
          executorUuid);
    }
    if (expectedReport.report.body.referrer !== undefined) {
      expectedReport.report.body.referrer = replaceFromRegexOrString(
          expectedReport.report.body.referrer, "EXECUTOR_UUID",
          executorUuid);
    }
  }
  if (expectedReport.report.url !== undefined) {
      expectedReport.report.url = replaceFromRegexOrString(
          expectedReport.report.url, "EXECUTOR_UUID", executorUuid);
  }
  return expectedReport;
}

function replaceTokensInReceivedReport(str) {
  return str.replace(/.{8}-.{4}-.{4}-.{4}-.{12}/g, `(uuid)`);
}

// Run a test (such as coop_coep_test from ./common.js) then check that all
// expected reports are present.
async function reportingTest(testFunction, executorToken, expectedReports) {
  await new Promise(testFunction);
  expectedReports = Array.from(
      expectedReports,
      report => replaceValuesInExpectedReport(report, executorToken) );
  await Promise.all(Array.from(expectedReports, checkForExpectedReport));
}

function getReportEndpoints(host) {
  result = "";
  reportEndpoints.forEach(
    reportEndpoint => {
      let reportToJSON = {
        'group': `${reportEndpoint.name}`,
        'max_age': 3600,
        'endpoints': [
          {
            'url': `${host}/reporting/resources/report.py?reportID=${reportEndpoint.reportID}`
          },
        ]
      };
      result += JSON.stringify(reportToJSON)
                        .replace(/,/g, '\\,')
                        .replace(/\(/g, '\\\(')
                        .replace(/\)/g, '\\\)=')
                + "\\,";
    }
  );
  return result.slice(0, -2);
}

function navigationReportingTest(testName, host, coop, coep, coopRo, coepRo,
    expectedReports ){
  const executorToken = token();
  const callbackToken = token();
  promise_test(async t => {
    await reportingTest( async resolve => {
      const openee_url = host.origin + executor_path +
      `|header(report-to,${encodeURIComponent(getReportEndpoints(host.origin))})` +
      `|header(Cross-Origin-Opener-Policy,${encodeURIComponent(coop)})` +
      `|header(Cross-Origin-Embedder-Policy,${encodeURIComponent(coep)})` +
      `|header(Cross-Origin-Opener-Policy-Report-Only,${encodeURIComponent(coopRo)})` +
      `|header(Cross-Origin-Embedder-Policy-Report-Only,${encodeURIComponent(coepRo)})`+
      `&uuid=${executorToken}`;
      const openee = window.open(openee_url);
      const uuid = token();
      t.add_cleanup(() => send(uuid, "window.close()"));

      // 1. Make sure the new document is loaded.
      send(executorToken, `
        send("${callbackToken}", "Ready");
      `);
      let reply = await receive(callbackToken);
      assert_equals(reply, "Ready");
      resolve();
    }, executorToken, expectedReports);
  }, `coop reporting test ${testName} to ${host.name} with ${coop}, ${coep}, ${coopRo}, ${coepRo}`);
}

// Run an array of reporting tests then verify there's no reports that were not
// expected.
// Tests' elements contain: host, coop, coep, coop-report-only,
// coep-report-only, expectedReports.
// See isObjectAsExpected for explanations regarding the matching behavior.
async function runNavigationReportingTests(testName, tests) {
  await clearReportsOnServer();
  tests.forEach(test => {
    navigationReportingTest(testName, ...test);
  });
  verifyRemainingReports();
}


function verifyRemainingReports() {
  promise_test(t => {
    return Promise.all(reportEndpoints.map(async (endpoint) => {
      await pollReports(endpoint);
      assert_equals(endpoint.reports.length, 0, `${endpoint.name} should be empty`);
    }));
  }, "verify remaining reports");
}

const receiveReport = async function(uuid, type) {
  while(true) {
    let reports = await Promise.race([
      receive(uuid),
      new Promise(resolve => {
        step_timeout(resolve, 1000, "timeout");
      })
    ]);
    if (reports == "timeout")
      return "timeout";
    reports = JSON.parse(reports);

    for(report of reports) {
      if (report?.body?.type == type)
        return report;
    }
  }
}

// Build a set of headers to tests the reporting API. This defines a set of
// matching 'Report-To', 'Cross-Origin-Opener-Policy' and
// 'Cross-Origin-Opener-Policy-Report-Only' headers.
const reportToHeaders = function(uuid) {
  const report_endpoint_url = dispatcher_path + `?uuid=${uuid}`;
  let reportToJSON = {
    'group': `${uuid}`,
    'max_age': 3600,
    'endpoints': [
      {'url': report_endpoint_url.toString()},
    ]
  };
  reportToJSON = JSON.stringify(reportToJSON)
                     .replace(/,/g, '\\,')
                     .replace(/\(/g, '\\\(')
                     .replace(/\)/g, '\\\)=');

  return {
    header: `|header(report-to,${reportToJSON})`,
    coopSameOriginHeader: `|header(Cross-Origin-Opener-Policy,same-origin%3Breport-to="${uuid}")`,
    coopSameOriginAllowPopupsHeader: `|header(Cross-Origin-Opener-Policy,same-origin-allow-popups%3Breport-to="${uuid}")`,
    coopReportOnlySameOriginHeader: `|header(Cross-Origin-Opener-Policy-Report-Only,same-origin%3Breport-to="${uuid}")`,
    coopReportOnlySameOriginAllowPopupsHeader: `|header(Cross-Origin-Opener-Policy-Report-Only,same-origin-allow-popups%3Breport-to="${uuid}")`,
  };
};
