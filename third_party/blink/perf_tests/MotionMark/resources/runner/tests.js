var Headers = {
    testName: [
        {
            title: "<span onclick='benchmarkController.showDebugInfo()'>" + Strings.text.testName + "</span>",
            text: Strings.text.testName
        }
    ],
    score: [
        {
            title: Strings.text.score,
            text: Strings.json.score
        }
    ],
    details: [
        {
            title: "&nbsp;",
            text: function(data) {
                var bootstrap = data[Strings.json.complexity][Strings.json.bootstrap];
                return "<span>±" + (Statistics.largestDeviationPercentage(bootstrap.confidenceLow, bootstrap.median, bootstrap.confidenceHigh) * 100).toFixed(2) + "%</span>";
            }
        }
    ]
};

var Suite = function(name, tests) {
    this.name = name;
    this.tests = tests;
};

var Suites = [];

Suites.push(new Suite("MotionMark",
    [
        {
            url: "core/multiply.html",
            name: "Multiply"
        },
        {
            url: "core/canvas-stage.html?pathType=arcs",
            name: "Canvas Arcs"
        },
        {
            url: "core/leaves.html",
            name: "Leaves"
        },
        {
            url: "core/canvas-stage.html?pathType=linePath",
            name: "Paths"
        },
        {
            url: "core/canvas-stage.html?pathType=line&lineCap=square",
            name: "Canvas Lines"
        },
        {
            url: "core/image-data.html",
            name: "Images"
        },
        {
            url: "core/design.html",
            name: "Design"
        },
        {
            url: "core/suits.html",
            name: "Suits"
        },
    ]
));

function suiteFromName(name)
{
    return Suites.find(function(suite) { return suite.name == name; });
}

function testFromName(suite, name)
{
    return suite.tests.find(function(test) { return test.name == name; });
}
