var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    }
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var assert = function (condition, msg) {
    if (!condition) {
        throw new AssertionError(msg);
    }
};
var AssertionError = /** @class */ (function (_super) {
    __extends(AssertionError, _super);
    function AssertionError(msg) {
        return _super.call(this, msg) || this;
    }
    return AssertionError;
}(Error));
assert(2 + 2 === 4, 'The laws of mathematics have been violated.');
//# sourceMappingURL=sourcemap-src-not-loaded.js.map