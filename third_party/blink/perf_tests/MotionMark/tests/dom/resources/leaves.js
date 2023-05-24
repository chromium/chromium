/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
(function() {

var SuperLeaf = window.Leaf;
var SimpleLeaf = Utilities.createSubclass(SuperLeaf,
    function(stage)
    {
        SuperLeaf.call(this, stage);
    }, {

    sizeMinimum: 25,
    sizeRange: 0,
    usesOpacity: false,

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
    }
});

var ScaleLeaf = Utilities.createSubclass(SuperLeaf,
    function(stage)
    {
        SuperLeaf.call(this, stage);
    }, {

    sizeMinimum: 20,
    sizeRange: 30,
    usesOpacity: false,

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
    }
});

var OpacityLeaf = Utilities.createSubclass(SuperLeaf,
    function(stage)
    {
        SuperLeaf.call(this, stage);
    }, {

    sizeMinimum: 25,
    sizeRange: 0,
    usesOpacity: true,

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
        this.element.style.opacity = this._opacity;
    }
});


var LeavesBenchmark = window.benchmarkClass;
var LeavesDerivedBenchmark = Utilities.createSubclass(LeavesBenchmark,
    function(options)
    {
        switch (options["style"]) {
        case "simple":
            window.Leaf = SimpleLeaf;
            break;
        case "scale":
            window.Leaf = ScaleLeaf;
            break;
        case "opacity":
            window.Leaf = OpacityLeaf;
            break;
        }
        LeavesBenchmark.call(this, options);
    }
);

window.benchmarkClass = LeavesDerivedBenchmark;

})();
