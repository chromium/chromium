/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
Utilities =
{
    _parse: function(str, sep)
    {
        var output = {};
        str.split(sep).forEach(function(part) {
            var item = part.split("=");
            var value = decodeURIComponent(item[1]);
            if (value[0] == "'" )
                output[item[0]] = value.substr(1, value.length - 2);
            else
                output[item[0]] = value;
          });
        return output;
    },

    parseParameters: function()
    {
        return this._parse(window.location.search.substr(1), "&");
    },

    parseArguments: function(str)
    {
        return this._parse(str, " ");
    },

    extendObject: function(obj1, obj2)
    {
        for (var attrname in obj2)
            obj1[attrname] = obj2[attrname];
        return obj1;
    },

    copyObject: function(obj)
    {
        return this.extendObject({}, obj);
    },

    mergeObjects: function(obj1, obj2)
    {
        return this.extendObject(this.copyObject(obj1), obj2);
    },

    createClass: function(classConstructor, classMethods)
    {
        classConstructor.prototype = classMethods;
        return classConstructor;
    },

    createSubclass: function(superclass, classConstructor, classMethods)
    {
        classConstructor.prototype = Object.create(superclass.prototype);
        classConstructor.prototype.constructor = classConstructor;
        if (classMethods)
            Utilities.extendObject(classConstructor.prototype, classMethods);
        return classConstructor;
    },

    createElement: function(name, attrs, parentElement)
    {
        var element = document.createElement(name);

        for (var key in attrs)
            element.setAttribute(key, attrs[key]);

        parentElement.appendChild(element);
        return element;
    },

    createSVGElement: function(name, attrs, xlinkAttrs, parentElement)
    {
        const svgNamespace = "http://www.w3.org/2000/svg";
        const xlinkNamespace = "http://www.w3.org/1999/xlink";

        var element = document.createElementNS(svgNamespace, name);

        for (var key in attrs)
            element.setAttribute(key, attrs[key]);

        for (var key in xlinkAttrs)
            element.setAttributeNS(xlinkNamespace, key, xlinkAttrs[key]);

        parentElement.appendChild(element);
        return element;
    },

    browserPrefix: function()
    {
        if (this._browserPrefix !== undefined)
            return this._browserPrefix;

        // Get the HTML element's CSSStyleDeclaration
        var styles = window.getComputedStyle(document.documentElement, '');

        // Convert the styles list to an array
        var stylesArray = Array.prototype.slice.call(styles);

        // Concatenate all the styles in one big string
        var stylesString = stylesArray.join('');

        // Search the styles string for a known prefix type, settle on Opera if none is found.
        var prefixes = stylesString.match(/-(moz|webkit|ms)-/) || (styles.OLink === '' && ['', 'o']);

        // prefixes has two elements; e.g. for webkit it has ['-webkit-', 'webkit'];
        var prefix = prefixes[1];

        // Have 'O' before 'Moz' in the string so it is matched first.
        var dom = ('WebKit|O|Moz|MS').match(new RegExp(prefix, 'i'))[0];

        // Return all the required prefixes.
        this._browserPrefix = {
            dom: dom,
            lowercase: prefix,
            css: '-' + prefix + '-',
            js: prefix[0].toUpperCase() + prefix.substr(1)
        };

        return this._browserPrefix;
    },

    setElementPrefixedProperty: function(element, property, value)
    {
        element.style[property] = element.style[this.browserPrefix().js + property[0].toUpperCase() + property.substr(1)] = value;
    },

    stripUnwantedCharactersForURL: function(inputString)
    {
        return inputString.replace(/\W/g, '');
    },

    convertObjectToQueryString: function(object)
    {
        var queryString = [];
        for (var property in object) {
            if (object.hasOwnProperty(property))
                queryString.push(encodeURIComponent(property) + "=" + encodeURIComponent(object[property]));
        }
        return "?" + queryString.join("&");
    },

    convertQueryStringToObject: function(queryString)
    {
        queryString = queryString.substring(1);
        if (!queryString)
            return null;

        var object = {};
        queryString.split("&").forEach(function(parameter) {
            var components = parameter.split("=");
            object[components[0]] = components[1];
        });
        return object;
    },

    progressValue: function(value, min, max)
    {
        return (value - min) / (max - min);
    },

    lerp: function(value, min, max)
    {
        return min + (max - min) * value;
    },

    toFixedNumber: function(number, precision)
    {
        if (number.toFixed)
            return Number(number.toFixed(precision));
        return number;
    }
};

Array.prototype.swap = function(i, j)
{
    var t = this[i];
    this[i] = this[j];
    this[j] = t;
    return this;
}

if (!Array.prototype.fill) {
    Array.prototype.fill = function(value) {
        if (this == null)
            throw new TypeError('Array.prototype.fill called on null or undefined');

        var object = Object(this);
        var len = parseInt(object.length, 10);
        var start = arguments[1];
        var relativeStart = parseInt(start, 10) || 0;
        var k = relativeStart < 0 ? Math.max(len + relativeStart, 0) : Math.min(relativeStart, len);
        var end = arguments[2];
        var relativeEnd = end === undefined ? len : (parseInt(end) || 0) ;
        var final = relativeEnd < 0 ? Math.max(len + relativeEnd, 0) : Math.min(relativeEnd, len);

        for (; k < final; k++)
            object[k] = value;

        return object;
    };
}

if (!Array.prototype.find) {
    Array.prototype.find = function(predicate) {
        if (this == null)
            throw new TypeError('Array.prototype.find called on null or undefined');
        if (typeof predicate !== 'function')
            throw new TypeError('predicate must be a function');

        var list = Object(this);
        var length = list.length >>> 0;
        var thisArg = arguments[1];
        var value;

        for (var i = 0; i < length; i++) {
            value = list[i];
            if (predicate.call(thisArg, value, i, list))
                return value;
        }
        return undefined;
    };
}

Array.prototype.shuffle = function()
{
    for (var index = this.length - 1; index >= 0; --index) {
        var randomIndex = Math.floor(Math.random() * (index + 1));
        this.swap(index, randomIndex);
    }
    return this;
}

Point = Utilities.createClass(
    function(x, y)
    {
        this.x = x;
        this.y = y;
    }, {

    // Used when the point object is used as a size object.
    get width()
    {
        return this.x;
    },

    // Used when the point object is used as a size object.
    get height()
    {
        return this.y;
    },

    // Used when the point object is used as a size object.
    get center()
    {
        return new Point(this.x / 2, this.y / 2);
    },

    str: function()
    {
        return "x = " + this.x + ", y = " + this.y;
    },

    add: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x + other, this.y + other);
        return new Point(this.x + other.x, this.y + other.y);
    },

    subtract: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x - other, this.y - other);
        return new Point(this.x - other.x, this.y - other.y);
    },

    multiply: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x * other, this.y * other);
        return new Point(this.x * other.x, this.y * other.y);
    },

    move: function(angle, velocity, timeDelta)
    {
        return this.add(Point.pointOnCircle(angle, velocity * (timeDelta / 1000)));
    },

    length: function() {
        return Math.sqrt( this.x * this.x + this.y * this.y );
    },

    normalize: function() {
        var l = Math.sqrt( this.x * this.x + this.y * this.y );
        this.x /= l;
        this.y /= l;
        return this;
    }
});

Utilities.extendObject(Point, {
    zero: new Point(0, 0),

    pointOnCircle: function(angle, radius)
    {
        return new Point(radius * Math.cos(angle), radius * Math.sin(angle));
    },

    pointOnEllipse: function(angle, radiuses)
    {
        return new Point(radiuses.x * Math.cos(angle), radiuses.y * Math.sin(angle));
    },

    elementClientSize: function(element)
    {
        var rect = element.getBoundingClientRect();
        return new Point(rect.width, rect.height);
    }
});

Insets = Utilities.createClass(
    function(top, right, bottom, left)
    {
        this.top = top;
        this.right = right;
        this.bottom = bottom;
        this.left = left;
    }, {

    get width()
    {
        return this.left + this.right;
    },

    get height()
    {
        return this.top + this.bottom;
    },

    get size()
    {
        return new Point(this.width, this.height);
    }
});

Insets.elementPadding = function(element)
{
    var styles = window.getComputedStyle(element);
    return new Insets(
        parseFloat(styles.paddingTop),
        parseFloat(styles.paddingRight),
        parseFloat(styles.paddingBottom),
        parseFloat(styles.paddingTop));
}

UnitBezier = Utilities.createClass(
    function(point1, point2)
    {
        // First and last points in the Bézier curve are assumed to be (0,0) and (!,1)
        this._c = point1.multiply(3);
        this._b = point2.subtract(point1).multiply(3).subtract(this._c);
        this._a = new Point(1, 1).subtract(this._c).subtract(this._b);
    }, {

    epsilon: 1e-5,
    derivativeEpsilon: 1e-6,

    solve: function(x)
    {
        return this.sampleY(this.solveForT(x));
    },

    sampleX: function(t)
    {
        return ((this._a.x * t + this._b.x) * t + this._c.x) * t;
    },

    sampleY: function(t)
    {
        return ((this._a.y * t + this._b.y) * t + this._c.y) * t;
    },

    sampleDerivativeX: function(t)
    {
        return(3 * this._a.x * t + 2 * this._b.x) * t + this._c.x;
    },

    solveForT: function(x)
    {
        var t0, t1, t2, x2, d2, i;

        for (t2 = x, i = 0; i < 8; ++i) {
            x2 = this.sampleX(t2) - x;
            if (Math.abs(x2) < this.epsilon)
                return t2;
            d2 = this.sampleDerivativeX(t2);
            if (Math.abs(d2) < this.derivativeEpsilon)
                break;
            t2 = t2 - x2 / d2;
        }

        t0 = 0;
        t1 = 1;
        t2 = x;

        if (t2 < t0)
            return t0;
        if (t2 > t1)
            return t1;

        while (t0 < t1) {
            x2 = this.sampleX(t2);
            if (Math.abs(x2 - x) < this.epsilon)
                return t2;
            if (x > x2)
                t0 = t2;
            else
                t1 = t2;
            t2 = (t1 - t0) * .5 + t0;
        }

        return t2;
    }
});

SimplePromise = Utilities.createClass(
    function()
    {
        this._chainedPromise = null;
        this._callback = null;
    }, {

    then: function (callback)
    {
        if (this._callback)
            throw "SimplePromise doesn't support multiple calls to then";

        this._callback = callback;
        this._chainedPromise = new SimplePromise;

        if (this._resolved)
            this.resolve(this._resolvedValue);

        return this._chainedPromise;
    },

    resolve: function (value)
    {
        if (!this._callback) {
            this._resolved = true;
            this._resolvedValue = value;
            return;
        }

        var result = this._callback(value);
        if (result instanceof SimplePromise) {
            var chainedPromise = this._chainedPromise;
            result.then(function (result) { chainedPromise.resolve(result); });
        } else
            this._chainedPromise.resolve(result);
    }
});

var Heap = Utilities.createClass(
    function(maxSize, compare)
    {
        this._maxSize = maxSize;
        this._compare = compare;
        this._size = 0;
        this._values = new Array(this._maxSize);
    }, {

    // This is a binary heap represented in an array. The root element is stored
    // in the first element in the array. The root is followed by its two children.
    // Then its four grandchildren and so on. So every level in the binary heap is
    // doubled in the following level. Here is an example of the node indices and
    // how they are related to their parents and children.
    // ===========================================================================
    //              0       1       2       3       4       5       6
    // PARENT       -1      0       0       1       1       2       2
    // LEFT         1       3       5       7       9       11      13
    // RIGHT        2       4       6       8       10      12      14
    // ===========================================================================
    _parentIndex: function(i)
    {
        return i > 0 ? Math.floor((i - 1) / 2) : -1;
    },

    _leftIndex: function(i)
    {
        var leftIndex = i * 2 + 1;
        return leftIndex < this._size ? leftIndex : -1;
    },

    _rightIndex: function(i)
    {
        var rightIndex = i * 2 + 2;
        return rightIndex < this._size ? rightIndex : -1;
    },

    // Return the child index that may violate the heap property at index i.
    _childIndex: function(i)
    {
        var left = this._leftIndex(i);
        var right = this._rightIndex(i);

        if (left != -1 && right != -1)
            return this._compare(this._values[left], this._values[right]) > 0 ? left : right;

        return left != -1 ? left : right;
    },

    init: function()
    {
        this._size = 0;
    },

    top: function()
    {
        return this._size ? this._values[0] : NaN;
    },

    push: function(value)
    {
        if (this._size == this._maxSize) {
            // If size is bounded and the new value can be a parent of the top()
            // if the size were unbounded, just ignore the new value.
            if (this._compare(value, this.top()) > 0)
                return;
            this.pop();
        }
        this._values[this._size++] = value;
        this._bubble(this._size - 1);
    },

    pop: function()
    {
        if (!this._size)
            return NaN;

        this._values[0] = this._values[--this._size];
        this._sink(0);
    },

    _bubble: function(i)
    {
        // Fix the heap property at index i given that parent is the only node that
        // may violate the heap property.
        for (var pi = this._parentIndex(i); pi != -1; i = pi, pi = this._parentIndex(pi)) {
            if (this._compare(this._values[pi], this._values[i]) > 0)
                break;

            this._values.swap(pi, i);
        }
    },

    _sink: function(i)
    {
        // Fix the heap property at index i given that each of the left and the right
        // sub-trees satisfies the heap property.
        for (var ci = this._childIndex(i); ci != -1; i = ci, ci = this._childIndex(ci)) {
            if (this._compare(this._values[i], this._values[ci]) > 0)
                break;

            this._values.swap(ci, i);
        }
    },

    str: function()
    {
        var out = "Heap[" + this._size + "] = [";
        for (var i = 0; i < this._size; ++i) {
            out += this._values[i];
            if (i < this._size - 1)
                out += ", ";
        }
        return out + "]";
    },

    values: function(size) {
        // Return the last "size" heap elements values.
        var values = this._values.slice(0, this._size);
        return values.sort(this._compare).slice(0, Math.min(size, this._size));
    }
});

Utilities.extendObject(Heap, {
    createMinHeap: function(maxSize)
    {
        return new Heap(maxSize, function(a, b) { return b - a; });
    },

    createMaxHeap: function(maxSize) {
        return new Heap(maxSize, function(a, b) { return a - b; });
    }
});

var SampleData = Utilities.createClass(
    function(fieldMap, data)
    {
        this.fieldMap = fieldMap || {};
        this.data = data || [];
    }, {

    get length()
    {
        return this.data.length;
    },

    addField: function(name, index)
    {
        this.fieldMap[name] = index;
    },

    push: function(datum)
    {
        this.data.push(datum);
    },

    sort: function(sortFunction)
    {
        this.data.sort(sortFunction);
    },

    slice: function(begin, end)
    {
        return new SampleData(this.fieldMap, this.data.slice(begin, end));
    },

    forEach: function(iterationFunction)
    {
        this.data.forEach(iterationFunction);
    },

    createDatum: function()
    {
        return [];
    },

    getFieldInDatum: function(datum, fieldName)
    {
        if (typeof datum === 'number')
            datum = this.data[datum];
        return datum[this.fieldMap[fieldName]];
    },

    setFieldInDatum: function(datum, fieldName, value)
    {
        if (typeof datum === 'number')
            datum = this.data[datum];
        return datum[this.fieldMap[fieldName]] = value;
    },

    at: function(index)
    {
        return this.data[index];
    },

    toArray: function()
    {
        var array = [];

        this.data.forEach(function(datum) {
            var newDatum = {};
            array.push(newDatum);

            for (var fieldName in this.fieldMap) {
                var value = this.getFieldInDatum(datum, fieldName);
                if (value !== null && value !== undefined)
                    newDatum[fieldName] = value;
            }
        }, this);

        return array;
    }
});
