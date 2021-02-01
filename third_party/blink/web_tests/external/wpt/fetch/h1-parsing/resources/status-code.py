def main(request, response):
    output = "HTTP/1.1 "
    output += request.GET.first("input")
    output += "\n" + "header-parsing: is sad" + "\n"
    response.writer.write(output)
    response.close_connection = True
