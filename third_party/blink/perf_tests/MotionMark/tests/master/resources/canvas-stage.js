/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
SimpleCanvasStage = Utilities.createSubclass(Stage,
    function(canvasObject)
    {
        Stage.call(this);
        this._canvasObject = canvasObject;
        this.objects = [];
        this.offsetIndex = 0;
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this.offsetIndex = Math.min(this.offsetIndex - count, this.objects.length);
            return;
        }

        var newIndex = this.offsetIndex - count;
        if (newIndex < 0) {
            this.offsetIndex = 0;
            newIndex = -newIndex;
            for (var i = 0; i < newIndex; ++i) {
                if (this._canvasObject.constructor === Array)
                    this.objects.push(new (Stage.randomElementInArray(this._canvasObject))(this));
                else
                    this.objects.push(new this._canvasObject(this));
            }
        } else
            this.offsetIndex = newIndex;
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        for (var i = this.offsetIndex, length = this.objects.length; i < length; ++i)
            this.objects[i].draw(context);
    },

    complexity: function()
    {
        return this.objects.length - this.offsetIndex;
    }
});
