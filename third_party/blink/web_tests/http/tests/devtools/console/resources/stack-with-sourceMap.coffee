window.letsFailWithStack = ->
    console.log((new Error()).stack)

window.letsFailWithStack.displayName = "letsFailWithStack(a:1:2)"

class Failure
    letsFailWithStackInEval: ->
        eval("letsFailWithStack()");

window.failure = ->
    failure = new Failure
    failure.letsFailWithStackInEval.displayName = "letsFailWithStackInEval(a)"
    failure.letsFailWithStackInEval()
