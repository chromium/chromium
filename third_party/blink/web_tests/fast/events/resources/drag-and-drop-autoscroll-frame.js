function $(id) { return document.getElementById(id); }

var lastScrollLeft;
var lastScrollTop;

window.onload = function() {
    const test = async_test("DragAndDrop Autoscroll");
    test.add_cleanup(() => {
        eventSender.mouseUp();
    });

    var draggable = $('draggable');
    var scrollBarWidth = $('scrollbars').offsetWidth - $('scrollbars').firstChild.offsetWidth;
    var scrollBarHeight = $('scrollbars').offsetHeight - $('scrollbars').firstChild.offsetHeight;

    var scroller = $('scrollable');
    var scrollerRect;
    if (scroller) {
      scrollerRect = scroller.getBoundingClientRect();
    } else {
      scroller = document.scrollingElement;
      scrollerRect = {
          left: 0,
          top: 0,
          right: window.innerWidth,
          bottom: window.innerHeight,
      };
    }

    var eastX = scrollerRect.right - scrollBarWidth - 10;
    var northY = scrollerRect.top + 10;
    var southY= scrollerRect.bottom - scrollBarHeight - 10;
    var westX = scrollerRect.left + 10;

    function moveTo(newState, x, y)
    {
        state = newState;
        lastScrollLeft = scroller.scrollLeft;
        lastScrollTop = scroller.scrollTop;
        eventSender.mouseMoveTo(x, y);
    }

    var state = 'START';
    function process(event)
    {
        switch (state) {
        case 'NE':
            test.step(() => {
              assert_greater_than(
                  scroller.scrollLeft,
                  lastScrollLeft,
                  "NE should scroll right");
              assert_less_than(
                  scroller.scrollTop,
                  lastScrollTop,
                  "NE should scroll up");
            });
            moveTo('SE', eastX, southY);
            break;
        case 'SE':
            test.step(() => {
              assert_greater_than(
                  scroller.scrollLeft,
                  lastScrollLeft,
                  "SE should scroll right");
              assert_greater_than(
                  scroller.scrollTop,
                  lastScrollTop,
                  "SE should scroll down");
            });
            moveTo('SW', westX, southY);
            break;
        case 'SW':
            test.step(() => {
              assert_less_than(
                  scroller.scrollLeft,
                  lastScrollLeft,
                  "SW should scroll left");
              assert_greater_than(
                  scroller.scrollTop,
                  lastScrollTop,
                  "SW should scroll down");
            });
            moveTo('NW', westX, northY);
            break;
        case 'NW':
            test.step(() => {
              assert_less_than(
                  scroller.scrollLeft,
                  lastScrollLeft,
                  "NW should scroll left");
              assert_less_than(
                  scroller.scrollTop,
                  lastScrollTop,
                  "NW should scroll up");
            });
            test.done();
            state = 'DONE';
            break;
        case 'DONE':
            break;
        case 'START':
            moveTo('NE', eastX, northY);
            break;
        default:
            console.error('Bad state ' + state);
            break;
        }
    };

    if (!window.eventSender) {
        $("container").scrollIntoView();
        return;
    }

    if (scroller === document.scrollingElement)
      window.addEventListener('scroll', process);
    else
      scroller.addEventListener('scroll', process);

    $("container").scrollIntoView();

    eventSender.dragMode = false;

    // Grab draggable
    const draggable_rect = draggable.getBoundingClientRect();
    eventSender.mouseMoveTo(draggable_rect.left + 5, draggable_rect.top + 5);
    eventSender.mouseDown();
};
