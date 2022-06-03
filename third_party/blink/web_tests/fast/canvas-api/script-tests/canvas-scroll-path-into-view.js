var CShape = function(context, usePathObject) {
    this._context = context;

    if (usePathObject)
        this._path = new Path2D();
    else
        this._path = context;
};

CShape.prototype.usePathObject = function() {
    return this._path instanceof Path2D;
};

CShape.prototype.createShape = function() {
    // override
};

CShape.prototype.draw = function() {
    var context = this._context;
    var path = this._path;

    context.beginPath();
    this.createShape();
    if (this.usePathObject())
        context.stroke(path);
    else
        context.stroke();
};

CShape.prototype.scroll = function() {
    var context = this._context;
    var path = this._path;

    if (this.usePathObject())
        context.scrollPathIntoView(path);
    else
        context.scrollPathIntoView();
};

var overrideShape = function(overrideMethod) {
    var shape = function() {
        CShape.apply(this, arguments);
    };

    shape.prototype = new CShape;
    shape.prototype.createShape = overrideMethod;
    return shape;
};

var CRect = overrideShape(function() {
    var path = this._path;

    path.rect(-50, -50, 100, 100);
});

var CCapsule = overrideShape(function() {
    var path = this._path;

    path.arc(-35, 0, 50, Math.PI / 2, Math.PI * 1.5, false);
    path.lineTo(35, -50);
    path.arc(50, 0, 50, Math.PI * 1.5, Math.PI / 2, false);
    path.lineTo(-35, 50);
});

var CStar = overrideShape(function() {
    var path = this._path;

    path.moveTo(0, -50);
    path.lineTo(-15, -10);
    path.lineTo(-50, -10);
    path.lineTo(-15, 10);
    path.lineTo(-35, 50);
    path.lineTo(0, 20);
    path.lineTo(35, 50);
    path.lineTo(15, 10);
    path.lineTo(50, -10);
    path.lineTo(15, -10);
    path.lineTo(0, -50);
});

var CCurve = overrideShape(function() {
    var path = this._path;

    path.moveTo(-50, -50);
    path.bezierCurveTo(-50, 10, 50, 10, 50, 50);
});

var container = document.querySelector("div[class='container']");
var canvas = document.querySelector("canvas");
var context = canvas.getContext("2d");

function getRealValue(shape, degree, usePathObject) {
    // reset scroll
    container.scrollTop = 0;
    container.scrollLeft = 0;

    // draw shape stroke on canvas
    usePathObject = usePathObject == undefined || usePathObject == null ? false : true;
    var s = new shape(context, usePathObject);

    context.clearRect(0, 0, 400, 400);
    context.save();
    context.translate(200, 200);
    if (degree != 0 && degree != undefined && degree != null)
        context.rotate(Math.PI / 180 * degree);
    s.draw();
    s.scroll();
    context.stroke();
    context.restore();

    return Math.round(container.scrollTop);
}

function scrollTest(shape, degree, usePathObject, expectedValue) {
    var classes = [ "", "border", "padding", "padding border", "margin" ];
    var offset = [ 0, 500, 500, 1000, 500 ];

    for (var i = 0; i < classes.length; i++) {
        canvas.className = classes[i];
        window.testValue = getRealValue(shape, degree, usePathObject);
        shouldBe("testValue", String(expectedValue + offset[i]));
    }
}

description("Series of tests to ensure correct results of scrolling path into view on canvas");
debug("Test case 1: scrollPathIntoView() / CTM == identity");
scrollTest(CRect, 0, false, 150);
scrollTest(CCapsule, 0, false, 150);
scrollTest(CCurve, 0, false, 150);
scrollTest(CStar, 0, false, 150);
debug("");

debug("Test case 2: scrollPathIntoView() / CTM != identity");
scrollTest(CRect, 20, false, 136);
scrollTest(CCapsule, 42, false, 106);
scrollTest(CCurve, 63, false, 133);
scrollTest(CStar, 40, false, 160);
debug("");

debug("Test case 3: scrollPathIntoView(path2d) / CTM == identity");
scrollTest(CRect, 0, true, 150);
scrollTest(CCapsule, 0, true, 150);
scrollTest(CCurve, 0, true, 150);
scrollTest(CStar, 0, true, 150);
debug("");

debug("Test case 4: scrollPathIntoView(path2d) / CTM != identity");
scrollTest(CRect, 20, true, 136);
scrollTest(CCapsule, 42, true, 106);
scrollTest(CCurve, 63, true, 133);
scrollTest(CStar, 40, true, 160);
debug("");

debug("Test case 5: exceptions");
shouldThrow("context.scrollPathIntoView(null);");
shouldThrow("context.scrollPathIntoView([]);");
shouldThrow("context.scrollPathIntoView({});");
debug("");
